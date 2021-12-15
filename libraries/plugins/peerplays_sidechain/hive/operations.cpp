#include <graphene/peerplays_sidechain/hive/operations.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace peerplays_sidechain { namespace hive {

}}} // namespace graphene::peerplays_sidechain::hive

namespace fc {

static std::string trim_typename_namespace(const std::string &name) {
   auto start = name.find_last_of(':');
   start = (start == std::string::npos) ? 0 : start + 1;
   return name.substr(start);
}

struct from_static_variant_for_hive {
   fc::variant &var;
   from_static_variant_for_hive(fc::variant &dv) :
         var(dv) {
   }

   typedef void result_type;
   template <typename T>
   void operator()(const T &v) const {
      auto name = trim_typename_namespace(fc::get_typename<T>::name());
      fc::variant value;
      to_variant(v, value, 5);
      var = mutable_variant_object("type", name).set("value", value);
   }
};

struct to_static_variant_for_hive {
   const fc::variant &var;
   to_static_variant_for_hive(const fc::variant &dv) :
         var(dv) {
   }

   typedef void result_type;
   template <typename T>
   void operator()(T &v) const {
      from_variant(var, v, 5);
   }
};

struct get_static_variant_name {
   string &name;
   get_static_variant_name(string &n) :
         name(n) {
   }

   typedef void result_type;
   template <typename T>
   void operator()(T &v) const {
      name = trim_typename_namespace(fc::get_typename<T>::name());
   }
};

void to_variant(const graphene::peerplays_sidechain::hive::hive_operation &var, fc::variant &vo, uint32_t max_depth) {
   var.visit(from_static_variant_for_hive(vo));
}

void from_variant(const fc::variant &var, graphene::peerplays_sidechain::hive::hive_operation &vo, uint32_t max_depth) {
   static std::map<string, int64_t> to_tag = []() {
      std::map<string, int64_t> name_map;
      for (int i = 0; i < graphene::peerplays_sidechain::hive::hive_operation::count(); ++i) {
         graphene::peerplays_sidechain::hive::hive_operation tmp;
         tmp.set_which(i);
         string n;
         tmp.visit(get_static_variant_name(n));
         name_map[n] = i;
      }
      return name_map;
   }();

   auto ar = var.get_array();
   if (ar.size() < 2)
      return;
   auto var_second = ar[1];

   FC_ASSERT(var_second.is_object(), "Input data have to treated as object.");
   auto v_object = var_second.get_object();

   FC_ASSERT(v_object.contains("type"), "Type field doesn't exist.");
   FC_ASSERT(v_object.contains("value"), "Value field doesn't exist.");

   int64_t which = -1;

   if (v_object["type"].is_integer()) {
      which = v_object["type"].as_int64();
   } else {
      auto itr = to_tag.find(v_object["type"].as_string());
      FC_ASSERT(itr != to_tag.end(), "Invalid object name: ${n}", ("n", v_object["type"]));
      which = itr->second;
   }

   vo.set_which(which);
   vo.visit(fc::to_static_variant_for_hive(v_object["value"]));
}

} // namespace fc
