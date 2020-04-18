#include <boost/test/unit_test.hpp>
#include <graphene/peerplays_sidechain/bitcoin/bitcoin_address.hpp>

using namespace graphene::peerplays_sidechain::bitcoin;

BOOST_AUTO_TEST_SUITE(bitcoin_address_tests)

fc::ecc::public_key_data create_public_key_data(const std::vector<char> &public_key) {
   FC_ASSERT(public_key.size() == 33);
   fc::ecc::public_key_data key;
   for (size_t i = 0; i < 33; i++) {
      key.at(i) = public_key[i];
   }
   return key;
}

BOOST_AUTO_TEST_CASE(addresses_type_test) {
   // public_key
   std::string compressed("03df51984d6b8b8b1cc693e239491f77a36c9e9dfe4a486e9972a18e03610a0d22");
   BOOST_CHECK(bitcoin_address(compressed).get_type() == payment_type::P2PK);

   std::string uncompressed("04fe53c78e36b86aae8082484a4007b706d5678cabb92d178fc95020d4d8dc41ef44cfbb8dfa7a593c7910a5b6f94d079061a7766cbeed73e24ee4f654f1e51904");
   BOOST_CHECK(bitcoin_address(uncompressed).get_type() == payment_type::NULLDATA);

   // segwit_address
   std::string p2wpkh_mainnet("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
   BOOST_CHECK(bitcoin_address(p2wpkh_mainnet).get_type() == payment_type::P2WPKH);

   std::string p2wpkh_testnet("tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx");
   BOOST_CHECK(bitcoin_address(p2wpkh_testnet).get_type() == payment_type::P2WPKH);

   std::string p2wsh("bc1qc7slrfxkknqcq2jevvvkdgvrt8080852dfjewde450xdlk4ugp7szw5tk9");
   BOOST_CHECK(bitcoin_address(p2wsh).get_type() == payment_type::P2WSH);

   // base58
   std::string p2pkh_mainnet("17VZNX1SN5NtKa8UQFxwQbFeFc3iqRYhem");
   BOOST_CHECK(bitcoin_address(p2pkh_mainnet).get_type() == payment_type::P2PKH);

   std::string p2pkh_testnet("mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn");
   BOOST_CHECK(bitcoin_address(p2pkh_testnet).get_type() == payment_type::P2PKH);

   std::string p2sh_mainnet("3EktnHQD7RiAE6uzMj2ZifT9YgRrkSgzQX");
   BOOST_CHECK(bitcoin_address(p2sh_mainnet).get_type() == payment_type::P2SH);

   std::string p2sh_testnet("2MzQwSSnBHWHqSAqtTVQ6v47XtaisrJa1Vc");
   BOOST_CHECK(bitcoin_address(p2sh_testnet).get_type() == payment_type::P2SH);

   std::string p2sh_regtest1("2NAL3YhMF4VcbRQdectN8XPMJipvATGefTZ");
   BOOST_CHECK(bitcoin_address(p2sh_regtest1).get_type() == payment_type::P2SH);
}

BOOST_AUTO_TEST_CASE(addresses_raw_test) {
   // public_key
   std::string compressed("03df51984d6b8b8b1cc693e239491f77a36c9e9dfe4a486e9972a18e03610a0d22");
   bytes standard_compressed(parse_hex(compressed));
   BOOST_CHECK(bitcoin_address(compressed).get_raw_address() == standard_compressed);

   std::string uncompressed("04fe53c78e36b86aae8082484a4007b706d5678cabb92d178fc95020d4d8dc41ef44cfbb8dfa7a593c7910a5b6f94d079061a7766cbeed73e24ee4f654f1e51904");
   BOOST_CHECK(bitcoin_address(uncompressed).get_raw_address() == bytes());

   // segwit_address
   std::string p2wpkh_mainnet("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
   bytes standard_p2wpkh_mainnet(parse_hex("751e76e8199196d454941c45d1b3a323f1433bd6"));
   BOOST_CHECK(bitcoin_address(p2wpkh_mainnet).get_raw_address() == standard_p2wpkh_mainnet);

   std::string p2wpkh_testnet("tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx");
   bytes standard_p2wpkh_testnet(parse_hex("751e76e8199196d454941c45d1b3a323f1433bd6"));
   BOOST_CHECK(bitcoin_address(p2wpkh_testnet).get_raw_address() == standard_p2wpkh_testnet);

   std::string p2wsh("bc1qc7slrfxkknqcq2jevvvkdgvrt8080852dfjewde450xdlk4ugp7szw5tk9");
   bytes standard_p2wsh(parse_hex("c7a1f1a4d6b4c1802a59631966a18359de779e8a6a65973735a3ccdfdabc407d"));
   BOOST_CHECK(bitcoin_address(p2wsh).get_raw_address() == standard_p2wsh);

   // base58
   std::string p2pkh_mainnet("17VZNX1SN5NtKa8UQFxwQbFeFc3iqRYhem");
   bytes standard_p2pkh_mainnet(parse_hex("47376c6f537d62177a2c41c4ca9b45829ab99083"));
   BOOST_CHECK(bitcoin_address(p2pkh_mainnet).get_raw_address() == standard_p2pkh_mainnet);

   std::string p2pkh_testnet("mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn");
   bytes standard_p2pkh_testnet(parse_hex("243f1394f44554f4ce3fd68649c19adc483ce924"));
   BOOST_CHECK(bitcoin_address(p2pkh_testnet).get_raw_address() == standard_p2pkh_testnet);

   std::string p2sh_mainnet("3EktnHQD7RiAE6uzMj2ZifT9YgRrkSgzQX");
   bytes standard_p2sh_mainnet(parse_hex("8f55563b9a19f321c211e9b9f38cdf686ea07845"));
   BOOST_CHECK(bitcoin_address(p2sh_mainnet).get_raw_address() == standard_p2sh_mainnet);

   std::string p2sh_testnet("2MzQwSSnBHWHqSAqtTVQ6v47XtaisrJa1Vc");
   bytes standard_p2sh_testnet(parse_hex("4e9f39ca4688ff102128ea4ccda34105324305b0"));
   BOOST_CHECK(bitcoin_address(p2sh_testnet).get_raw_address() == standard_p2sh_testnet);
}

BOOST_AUTO_TEST_CASE(create_multisig_address_test) {

   std::vector<char> public_key1 = parse_hex("03db643710666b862e0a97f7edbe8ef40ec2c4a29ef995c431c21ca85e35000010");
   std::vector<char> public_key2 = parse_hex("0320000d982c156a6f09df8c7674abddc2bb326533268ed03572916221b4417983");
   std::vector<char> public_key3 = parse_hex("033619e682149aef0c3e2dee3dc5107dd78cb2c14bf0bd25b59056259fbb37ec3f");

   std::vector<char> address = parse_hex("a91460cb986f0926e7c4ca1984ca9f56767da2af031e87");
   std::vector<char> redeem_script = parse_hex("522103db643710666b862e0a97f7edbe8ef40ec2c4a29ef995c431c21ca85e35000010210320000d982c156a6f09df8c7674abddc2bb326533268ed03572916221b441798321033619e682149aef0c3e2dee3dc5107dd78cb2c14bf0bd25b59056259fbb37ec3f53ae");

   fc::ecc::public_key_data key1 = create_public_key_data(public_key1);
   fc::ecc::public_key_data key2 = create_public_key_data(public_key2);
   fc::ecc::public_key_data key3 = create_public_key_data(public_key3);

   btc_multisig_address cma(2, {{son_id_type(1), public_key_type(key1)}, {son_id_type(2), public_key_type(key2)}, {son_id_type(3), public_key_type(key3)}});

   BOOST_CHECK(address == cma.raw_address);
   BOOST_CHECK(redeem_script == cma.redeem_script);
}

BOOST_AUTO_TEST_CASE(create_segwit_address_test) {
   // https://0bin.net/paste/nfnSf0HcBqBUGDto#7zJMRUhGEBkyh-eASQPEwKfNHgQ4D5KrUJRsk8MTPSa
   std::vector<char> public_key1 = parse_hex("03b3623117e988b76aaabe3d63f56a4fc88b228a71e64c4cc551d1204822fe85cb");
   std::vector<char> public_key2 = parse_hex("03dd823066e096f72ed617a41d3ca56717db335b1ea47a1b4c5c9dbdd0963acba6");
   std::vector<char> public_key3 = parse_hex("033d7c89bd9da29fa8d44db7906a9778b53121f72191184a9fee785c39180e4be1");

   std::vector<char> witness_script = parse_hex("0020b6744de4f6ec63cc92f7c220cdefeeb1b1bed2b66c8e5706d80ec247d37e65a1");

   fc::ecc::public_key_data key1 = create_public_key_data(public_key1);
   fc::ecc::public_key_data key2 = create_public_key_data(public_key2);
   fc::ecc::public_key_data key3 = create_public_key_data(public_key3);

   btc_multisig_segwit_address address(2, {{son_id_type(1), public_key_type(key1)}, {son_id_type(2), public_key_type(key2)}, {son_id_type(3), public_key_type(key3)}});
   BOOST_CHECK(address.get_witness_script() == witness_script);
   BOOST_CHECK(address.get_address() == "2NGU4ogScHEHEpReUzi9RB2ha58KAFnkFyk");
}

BOOST_AUTO_TEST_CASE(create_segwit_address_10_of_14_test) {
   std::vector<char> public_key1 = parse_hex("03456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef275");
   std::vector<char> public_key2 = parse_hex("02d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f4");
   std::vector<char> public_key3 = parse_hex("025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61");
   std::vector<char> public_key4 = parse_hex("0228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe9866");
   std::vector<char> public_key5 = parse_hex("037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f666");
   std::vector<char> public_key6 = parse_hex("02ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccf");
   std::vector<char> public_key7 = parse_hex("0317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7f");
   std::vector<char> public_key8 = parse_hex("0266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf84");
   std::vector<char> public_key9 = parse_hex("023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccf");
   std::vector<char> public_key10 = parse_hex("0229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e5");
   std::vector<char> public_key11 = parse_hex("024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8da");
   std::vector<char> public_key12 = parse_hex("03df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c1");
   std::vector<char> public_key13 = parse_hex("02bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8");
   std::vector<char> public_key14 = parse_hex("0287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae8");

   std::vector<char> witness_script = parse_hex("0020b70a52b55974d3c8c1a2055bf23e2d6421942135c7be1f786ad8cbce2f532cef");
   std::vector<char> redeem_script = parse_hex("5a2103456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef2752102d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f421025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61210228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe986621037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f6662102ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccf210317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7f210266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf8421023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccf210229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e521024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8da2103df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c12102bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8210287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae85eae");

   fc::ecc::public_key_data key1 = create_public_key_data(public_key1);
   fc::ecc::public_key_data key2 = create_public_key_data(public_key2);
   fc::ecc::public_key_data key3 = create_public_key_data(public_key3);
   fc::ecc::public_key_data key4 = create_public_key_data(public_key4);
   fc::ecc::public_key_data key5 = create_public_key_data(public_key5);
   fc::ecc::public_key_data key6 = create_public_key_data(public_key6);
   fc::ecc::public_key_data key7 = create_public_key_data(public_key7);
   fc::ecc::public_key_data key8 = create_public_key_data(public_key8);
   fc::ecc::public_key_data key9 = create_public_key_data(public_key9);
   fc::ecc::public_key_data key10 = create_public_key_data(public_key10);
   fc::ecc::public_key_data key11 = create_public_key_data(public_key11);
   fc::ecc::public_key_data key12 = create_public_key_data(public_key12);
   fc::ecc::public_key_data key13 = create_public_key_data(public_key13);
   fc::ecc::public_key_data key14 = create_public_key_data(public_key14);

   btc_multisig_segwit_address address(10, {
                                                 {son_id_type(1), public_key_type(key1)},
                                                 {son_id_type(2), public_key_type(key2)},
                                                 {son_id_type(3), public_key_type(key3)},
                                                 {son_id_type(4), public_key_type(key4)},
                                                 {son_id_type(5), public_key_type(key5)},
                                                 {son_id_type(6), public_key_type(key6)},
                                                 {son_id_type(7), public_key_type(key7)},
                                                 {son_id_type(8), public_key_type(key8)},
                                                 {son_id_type(9), public_key_type(key9)},
                                                 {son_id_type(10), public_key_type(key10)},
                                                 {son_id_type(11), public_key_type(key11)},
                                                 {son_id_type(12), public_key_type(key12)},
                                                 {son_id_type(13), public_key_type(key13)},
                                                 {son_id_type(14), public_key_type(key14)},
                                           });
   BOOST_CHECK(address.get_witness_script() == witness_script);
   BOOST_CHECK(address.get_redeem_script() == redeem_script);
   BOOST_CHECK(address.get_address() == "2MwhYhBrZeb6mTf1kaWcYfLBCCQoMqQybFZ");
}

BOOST_AUTO_TEST_CASE(create_segwit_address_11_of_15_test) {
   std::vector<char> public_key1 = parse_hex("03456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef275");
   std::vector<char> public_key2 = parse_hex("02d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f4");
   std::vector<char> public_key3 = parse_hex("025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61");
   std::vector<char> public_key4 = parse_hex("0228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe9866");
   std::vector<char> public_key5 = parse_hex("037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f666");
   std::vector<char> public_key6 = parse_hex("02ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccf");
   std::vector<char> public_key7 = parse_hex("0317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7f");
   std::vector<char> public_key8 = parse_hex("0266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf84");
   std::vector<char> public_key9 = parse_hex("023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccf");
   std::vector<char> public_key10 = parse_hex("0229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e5");
   std::vector<char> public_key11 = parse_hex("024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8da");
   std::vector<char> public_key12 = parse_hex("03df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c1");
   std::vector<char> public_key13 = parse_hex("02bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8");
   std::vector<char> public_key14 = parse_hex("0287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae8");
   std::vector<char> public_key15 = parse_hex("02053859d76aa375d6f343a60e3678e906c008015e32fe4712b1fd2b26473bdd73");

   fc::ecc::public_key_data key1 = create_public_key_data(public_key1);
   fc::ecc::public_key_data key2 = create_public_key_data(public_key2);
   fc::ecc::public_key_data key3 = create_public_key_data(public_key3);
   fc::ecc::public_key_data key4 = create_public_key_data(public_key4);
   fc::ecc::public_key_data key5 = create_public_key_data(public_key5);
   fc::ecc::public_key_data key6 = create_public_key_data(public_key6);
   fc::ecc::public_key_data key7 = create_public_key_data(public_key7);
   fc::ecc::public_key_data key8 = create_public_key_data(public_key8);
   fc::ecc::public_key_data key9 = create_public_key_data(public_key9);
   fc::ecc::public_key_data key10 = create_public_key_data(public_key10);
   fc::ecc::public_key_data key11 = create_public_key_data(public_key11);
   fc::ecc::public_key_data key12 = create_public_key_data(public_key12);
   fc::ecc::public_key_data key13 = create_public_key_data(public_key13);
   fc::ecc::public_key_data key14 = create_public_key_data(public_key14);
   fc::ecc::public_key_data key15 = create_public_key_data(public_key15);

   std::vector<char> witness_script = parse_hex("00205fcdce3c62b477553b8dc957a935adda8305cbd47a408f07d2cb867d588279eb");
   std::vector<char> redeem_script = parse_hex("5b2103456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef2752102d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f421025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61210228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe986621037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f6662102ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccf210317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7f210266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf8421023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccf210229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e521024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8da2103df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c12102bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8210287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae82102053859d76aa375d6f343a60e3678e906c008015e32fe4712b1fd2b26473bdd735fae");

   btc_multisig_segwit_address address(11, {{son_id_type(1), public_key_type(key1)}, {son_id_type(2), public_key_type(key2)}, {son_id_type(3), public_key_type(key3)}, {son_id_type(4), public_key_type(key4)}, {son_id_type(5), public_key_type(key5)}, {son_id_type(6), public_key_type(key6)}, {son_id_type(7), public_key_type(key7)}, {son_id_type(8), public_key_type(key8)}, {son_id_type(9), public_key_type(key9)}, {son_id_type(10), public_key_type(key10)}, {son_id_type(11), public_key_type(key11)}, {son_id_type(12), public_key_type(key12)}, {son_id_type(13), public_key_type(key13)}, {son_id_type(14), public_key_type(key14)}, {son_id_type(15), public_key_type(key15)}});

   BOOST_CHECK(address.get_witness_script() == witness_script);
   BOOST_CHECK(address.get_redeem_script() == redeem_script);
   BOOST_CHECK(address.get_address() == "2NAL3YhMF4VcbRQdectN8XPMJipvATGefTZ");
}

BOOST_AUTO_TEST_CASE(btc_weighted_multisig_address_test) {
   std::vector<fc::ecc::private_key> priv_old;
   for (uint32_t i = 0; i < 15; ++i) {
      const char *seed = reinterpret_cast<const char *>(&i);
      fc::sha256 h = fc::sha256::hash(seed, sizeof(i));
      priv_old.push_back(fc::ecc::private_key::generate_from_seed(h));
   }
   std::vector<fc::ecc::public_key> pub_old;
   for (auto &key : priv_old)
      pub_old.push_back(key.get_public_key());
   // key weights
   std::vector<std::pair<fc::ecc::public_key, uint16_t>> weights;
   for (uint16_t i = 0; i < 15; ++i)
      weights.push_back(std::make_pair(pub_old[i], i + 1));

   btc_weighted_multisig_address addr(weights, btc_weighted_multisig_address::network::testnet);
   BOOST_CHECK(addr.get_address() == "tb1qaeuy4c0qnudq5u2c8pndd7zyudal3g5eew7y9396592udxdcje4surl6cm");
}

BOOST_AUTO_TEST_CASE(create_1_or_2_of_15_segwit_address_test) {
   auto public_key1 = fc::ecc::public_key(create_public_key_data(parse_hex("03456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef275")));
   auto public_key2 = fc::ecc::public_key(create_public_key_data(parse_hex("02d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f4")));
   auto public_key3 = fc::ecc::public_key(create_public_key_data(parse_hex("025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61")));
   auto public_key4 = fc::ecc::public_key(create_public_key_data(parse_hex("0228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe9866")));
   auto public_key5 = fc::ecc::public_key(create_public_key_data(parse_hex("037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f666")));
   auto public_key6 = fc::ecc::public_key(create_public_key_data(parse_hex("02ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccf")));
   auto public_key7 = fc::ecc::public_key(create_public_key_data(parse_hex("0317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7f")));
   auto public_key8 = fc::ecc::public_key(create_public_key_data(parse_hex("0266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf84")));
   auto public_key9 = fc::ecc::public_key(create_public_key_data(parse_hex("023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccf")));
   auto public_key10 = fc::ecc::public_key(create_public_key_data(parse_hex("0229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e5")));
   auto public_key11 = fc::ecc::public_key(create_public_key_data(parse_hex("024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8da")));
   auto public_key12 = fc::ecc::public_key(create_public_key_data(parse_hex("03df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c1")));
   auto public_key13 = fc::ecc::public_key(create_public_key_data(parse_hex("02bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8")));
   auto public_key14 = fc::ecc::public_key(create_public_key_data(parse_hex("0287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae8")));
   auto public_key15 = fc::ecc::public_key(create_public_key_data(parse_hex("02053859d76aa375d6f343a60e3678e906c008015e32fe4712b1fd2b26473bdd73")));

   auto user_key = fc::ecc::public_key(create_public_key_data(parse_hex("0368dc31b2b547c74f52abfc67c7fc768c68115d8ab96430d9bb4996fa660121cd")));

   btc_one_or_m_of_n_multisig_address address(user_key, 2, {public_key1, public_key2, public_key3, public_key4, public_key5, public_key6, public_key7, public_key8, public_key9, public_key10, public_key11, public_key12, public_key13, public_key14, public_key15});

   std::vector<char> redeem_script = parse_hex("63210368dc31b2b547c74f52abfc67c7fc768c68115d8ab96430d9bb4996fa660121cdac67522103456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef2752102d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f421025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61210228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe986621037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f6662102ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccf210317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7f210266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf8421023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccf210229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e521024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8da2103df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c12102bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8210287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae82102053859d76aa375d6f343a60e3678e906c008015e32fe4712b1fd2b26473bdd735fae68");

   BOOST_CHECK(address.get_address() == "bcrt1qp8vplzrn7alzpjq8cw4ynd6xqzassmefrh48dzsj0krvkq85dywq9txkqr");
   BOOST_CHECK(address.get_redeem_script() == redeem_script);
}

BOOST_AUTO_TEST_CASE(create_1_or_multiweighted_seggwit_address_test) {
   auto public_key1 = fc::ecc::public_key(create_public_key_data(parse_hex("03456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef275")));
   auto public_key2 = fc::ecc::public_key(create_public_key_data(parse_hex("02d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f4")));
   auto public_key3 = fc::ecc::public_key(create_public_key_data(parse_hex("025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61")));
   auto public_key4 = fc::ecc::public_key(create_public_key_data(parse_hex("0228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe9866")));
   auto public_key5 = fc::ecc::public_key(create_public_key_data(parse_hex("037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f666")));
   auto public_key6 = fc::ecc::public_key(create_public_key_data(parse_hex("02ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccf")));
   auto public_key7 = fc::ecc::public_key(create_public_key_data(parse_hex("0317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7f")));
   auto public_key8 = fc::ecc::public_key(create_public_key_data(parse_hex("0266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf84")));
   auto public_key9 = fc::ecc::public_key(create_public_key_data(parse_hex("023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccf")));
   auto public_key10 = fc::ecc::public_key(create_public_key_data(parse_hex("0229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e5")));
   auto public_key11 = fc::ecc::public_key(create_public_key_data(parse_hex("024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8da")));
   auto public_key12 = fc::ecc::public_key(create_public_key_data(parse_hex("03df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c1")));
   auto public_key13 = fc::ecc::public_key(create_public_key_data(parse_hex("02bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8")));
   auto public_key14 = fc::ecc::public_key(create_public_key_data(parse_hex("0287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae8")));
   auto public_key15 = fc::ecc::public_key(create_public_key_data(parse_hex("02053859d76aa375d6f343a60e3678e906c008015e32fe4712b1fd2b26473bdd73")));

   auto user_pub_key = fc::ecc::public_key(create_public_key_data(parse_hex("0368dc31b2b547c74f52abfc67c7fc768c68115d8ab96430d9bb4996fa660121cd")));

   // key weights
   std::vector<std::pair<fc::ecc::public_key, uint16_t>> weights;
   weights.push_back(std::make_pair(public_key1, 1));
   weights.push_back(std::make_pair(public_key2, 1));
   weights.push_back(std::make_pair(public_key3, 1));
   weights.push_back(std::make_pair(public_key4, 1));
   weights.push_back(std::make_pair(public_key5, 1));
   weights.push_back(std::make_pair(public_key6, 1));
   weights.push_back(std::make_pair(public_key7, 1));
   weights.push_back(std::make_pair(public_key8, 1));
   weights.push_back(std::make_pair(public_key9, 1));
   weights.push_back(std::make_pair(public_key10, 1));
   weights.push_back(std::make_pair(public_key11, 1));
   weights.push_back(std::make_pair(public_key12, 1));
   weights.push_back(std::make_pair(public_key13, 1));
   weights.push_back(std::make_pair(public_key14, 1));
   weights.push_back(std::make_pair(public_key15, 1));

   btc_one_or_weighted_multisig_address addr(user_pub_key, weights);
   auto redeem_script = parse_hex("210368dc31b2b547c74f52abfc67c7fc768c68115d8ab96430d9bb4996fa660121cdac635167007c2103456772301e221026269d3095ab5cb623fc239835b583ae4632f99a15107ef275ac635193687c2102d67c26cf20153fe7625ca1454222d3b3aeb53b122d8a0f7d32a3dd4b2c2016f4ac635193687c21025f7cfda933516fd590c5a34ad4a68e3143b6f4155a64b3aab2c55fb851150f61ac635193687c210228155bb1ddcd11c7f14a2752565178023aa963f84ea6b6a052bddebad6fe9866ac635193687c21037500441cfb4484da377073459511823b344f1ef0d46bac1efd4c7c466746f666ac635193687c2102ef0d79bfdb99ab0be674b1d5d06c24debd74bffdc28d466633d6668cc281cccfac635193687c210317941e4219548682fb8d8e172f0a8ce4d83ce21272435c85d598558c8e060b7fac635193687c210266065b27f7e3d3ad45b471b1cd4e02de73fc4737dc2679915a45e293c5adcf84ac635193687c21023821cc3da7be9e8cdceb8f146e9ddd78a9519875ecc5b42fe645af690544bccfac635193687c210229ff2b2106b76c27c393e82d71c20eec32bcf1f0cf1a9aca8a237269a67ff3e5ac635193687c21024d113381cc09deb8a6da62e0470644d1a06de82be2725b5052668c8845a4a8daac635193687c2103df2462a5a2f681a3896f61964a65566ff77448be9a55a6da18506fd9c6c051c1ac635193687c2102bafba3096f546cc5831ce1e49ba7142478a659f2d689bbc70ed37235255172a8ac635193687c210287bcbd4f5d357f89a86979b386402445d7e9a5dccfd16146d1d2ab0dc2c32ae8ac635193687c2102053859d76aa375d6f343a60e3678e906c008015e32fe4712b1fd2b26473bdd73ac635193685aa268");

   BOOST_CHECK(addr.get_address() == "bcrt1qawr44trakzl4qev8ed88samt3g3g5mgcjppc6ffs5h4wyakqzavq6fkcc6");
   BOOST_CHECK(addr.get_redeem_script() == redeem_script);
}

BOOST_AUTO_TEST_SUITE_END()
