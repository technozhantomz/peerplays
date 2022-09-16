#! /usr/bin/perl

use Text::Wrap;
use IO::File;

require 'doxygen/perlmod/DoxyDocs.pm';

my($outputFileName) = @ARGV;
die "usage: $0 output_file_name" unless $outputFileName;
my $outFile = new IO::File($outputFileName, "w")
  or die "Error opening output file: $!";

my $fileHeader = <<'END';
#include <set>
#include <graphene/wallet/api_documentation.hpp>
#include <graphene/wallet/wallet.hpp>

namespace graphene { namespace wallet {
   namespace detail 
   {
      struct api_method_name_collector_visitor
      {
         std::set<std::string> method_names;

         template<typename R, typename... Args>
         void operator()( const char* name, std::function<R(Args...)>& memb )
         {
            method_names.emplace(name);
         }
      };
   }
  
   api_documentation::api_documentation()
   {
END
$outFile->print($fileHeader);

for my $class (@{$doxydocs->{classes}})
{
  if ($class->{name} eq 'graphene::wallet::wallet_api')
  {
    for my $member (@{$class->{public_methods}->{members}})
    {
      if ($member->{kind} eq 'function')
      {
        my @params = map { join(' ', cleanupDoxygenType($_->{type}), $_->{declaration_name}) } @{$member->{parameters}};
        my $callDescription = sprintf("%40s %s(%s)\n", cleanupDoxygenType($member->{type}), $member->{name}, join(', ', @params));
        my $escapedBriefDescription = "\"" . escapeStringForC($callDescription) . "\"";
        my %paramInfo = map { $_->{declaration_name} => { type => explainCType(cleanupDoxygenType($_->{type})) } } @{$member->{parameters}};
        my $escapedDetailedDescription = "\"\"\n";
        my $doc = $member->{detailed}->{doc};
        if ($doc)
        {
          my $briefDescr = formatDocComment($member->{brief}->{doc});  # get from the proper place
          unless ($briefDescr =~ /\w/)           # if not provided (API author forgot to add '@brief' comment),
          {
            for (my $i = 0; $i < @{$doc}; ++$i) # then look inside 'detailed' section
            {
              my $docElement = $doc->[$i];
              if ($docElement->{type} eq 'text' and $docElement->{content} =~ /\w+/)  # use first meaningful line as brief description
              {
                $briefDescr = $docElement->{content};
                $briefDescr =~ s/^\s+|\s+$//g;
                splice @{$doc}, $i, 1;  # this section shouldn't be used twice
                last;
              }
            }
          }

          my $cmdSyntax = $member->{name};
          my $cmdArgs = join(' ', map { $_->{declaration_name} } @{$member->{parameters}});
          $cmdSyntax .= " $cmdArgs" if $cmdArgs;

          my $docString;
          $docString .= $briefDescr;
          $docString .= "\n\n" . formatDocComment($doc, \%paramInfo, $cmdSyntax);

          for my $line (split(/\n/, $docString))
          {
            $escapedDetailedDescription .=  "                \"" .  escapeStringForC($line . "\n") . "\"\n";
          }
        }
        my $codeFragment = <<"END";
     {
        method_description this_method;
        this_method.method_name = "$member->{name}";
        this_method.brief_description = $escapedBriefDescription;
        this_method.detailed_description = $escapedDetailedDescription;
        method_descriptions.insert(this_method);
     }

END
        $outFile->print($codeFragment);
      }
    }
  }
}

my $fileFooter = <<'END';
      fc::api<wallet_api> tmp;
      detail::api_method_name_collector_visitor visitor;
      tmp->visit(visitor);
      for (auto iter = method_descriptions.begin(); iter != method_descriptions.end();)
        if (visitor.method_names.find(iter->method_name) == visitor.method_names.end())
          iter = method_descriptions.erase(iter);
        else
          ++iter;
   }

} } // end namespace graphene::wallet
END
$outFile->print($fileFooter);
$outFile->close();

sub cleanupDoxygenType
{
  my($type) = @_;
  $type =~ s/< /</g;
  $type =~ s/ >/>/g;
  return $type;
}

sub explainCType
{
  my($type) = @_;
  $type =~ s/\b\w+:://g;                    # remove namespaces
  $type =~ s/^(?:optional|api)<(.+)>$/$1/;  # disregard optional<> and some other templates
  $type =~ s/^const\s+(.+)/$1/;  # strip const modifier
  $type =~ s/^(.+)&/$1/;         # strip references
  $type =~ s/\s+$/$1/;
  $type =~ s/\b(u?int(8|16|32|64)_t|int|unsigned)\b/integer/;  # spare the user from width and signedness
  $type =~ s/\bbool\b/boolean/;      # they're not C++ people
  $type =~ s/^(?:vector|set|flat_set)<(.+)>$/[$1, ...]/;            # represent as JSon-like array notation
  $type =~ s/^(?:map|flat_map)<(.+)\s*,\s*(.+)>$/{$1 => $2, ...}/;  # same for map
  $type =~ s/^time_point_sec$/time, e.g. 2021-12-25T14:30:05/;
  return $type;
}

sub formatDocComment
{
  my($doc, $paramInfo, $cmdSyntax) = @_;

  my $bodyDocs = '';
  my $notes = '';
  my $see = '';
  my $paramDocs = '';
  my $returnDocs = '';

  for (my $i = 0; $i < @{$doc}; ++$i)
  {
    my $docElement = $doc->[$i];

    if ($docElement->{params})
    {
      $paramDocs .= "Parameters:\n";
      for my $parameter (@{$docElement->{params}})
      {
        my $declname = $parameter->{parameters}->[0]->{name};
        my $decltype = cleanupDoxygenType($paramInfo->{$declname}->{type});
        $paramDocs .= Text::Wrap::fill('    ', '        ', "$declname ($decltype):  " . formatDocComment($parameter->{doc})) . "\n";
      }
    }
    elsif ($docElement->{return})
    {
      $returnDocs .= "Returns:\n";
      $returnDocs .= Text::Wrap::fill('    ','        ', formatDocComment($docElement->{return})) . "\n";
    }
    elsif ($docElement->{note})
    {
      $notes .= Text::Wrap::fill('    ','        ', "Note: ".formatDocComment($docElement->{note})) . "\n";
    }
    elsif ($docElement->{see})
    {
      $see .= Text::Wrap::fill('    ','        ', "See: ".formatDocComment($docElement->{see})) . "\n";
    }
    elsif ($docElement->{type} eq 'text' or $docElement->{type} eq 'url')
    {
      $bodyDocs .= $docElement->{content};
    }
    elsif ($docElement->{type} eq 'parbreak')
    {
      $bodyDocs .= "\n\n";
    }
    elsif ($docElement->{type} eq 'style' and $docElement->{style} eq 'code')
    {
      $bodyDocs .= "'";
    }
  }

  $bodyDocs =~ s/^\s+|\s+$//g;
  $bodyDocs = Text::Wrap::fill('', '', $bodyDocs);

  $notes      =~ s/^\s+|\s+$//g;
  $see        =~ s/^\s+|\s+$//g;
  $paramDocs  =~ s/^\s+|\s+$//g;
  $returnDocs =~ s/^\s+|\s+$//g;

  my $cmdDocs;
  $cmdDocs = "Command:\n" . Text::Wrap::fill('    ','        ', $cmdSyntax) if $cmdSyntax;

  return join "\n\n", grep {$_} ($bodyDocs, $notes, $see, $cmdDocs, $paramDocs, $returnDocs);
}

sub escapeCharForCString
{
  my($char) = @_;
  return "\\a" if $char eq "\x07";
  return "\\b" if $char eq "\x08";
  return "\\f" if $char eq "\x0c";
  return "\\n" if $char eq "\x0a";
  return "\\r" if $char eq "\x0d";
  return "\\t" if $char eq "\x09";
  return "\\v" if $char eq "\x0b";
  return "\\\"" if $char eq "\x22";
  return "\\\\" if $char eq "\x5c";
  return $char;
}

sub escapeStringForC
{
  my($str) = @_;
  return join('', map { escapeCharForCString($_) } split('', $str));
}


