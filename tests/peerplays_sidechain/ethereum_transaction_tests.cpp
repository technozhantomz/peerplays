#include <boost/test/unit_test.hpp>

#include <fc/crypto/hex.hpp>
#include <graphene/peerplays_sidechain/ethereum/encoders.hpp>
#include <graphene/peerplays_sidechain/ethereum/decoders.hpp>
#include <graphene/peerplays_sidechain/ethereum/transaction.hpp>

using namespace graphene::peerplays_sidechain::ethereum;

BOOST_AUTO_TEST_SUITE(ethereum_transaction_tests)

BOOST_AUTO_TEST_CASE(withdrawal_encoder_test) {
   const auto tx = withdrawal_encoder::encode("5fbbb31be52608d2f52247e8400b7fcaa9e0bc12", 10000000000, "1.39.0");
   BOOST_CHECK_EQUAL(tx, "0xe088747b0000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc1200000000000000000000000000000000000000000000000000000002540be40000000000000000000000000000000000000000000000000000000000000000600000000000000000000000000000000000000000000000000000000000000006312E33392E300000000000000000000000000000000000000000000000000000");

   const auto tx1 = withdrawal_encoder::encode("5fbbb31be52608d2f52247e8400b7fcaa9e0bc12", 10000000000, "1.39.1");
   BOOST_CHECK_EQUAL(tx1, "0xe088747b0000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc1200000000000000000000000000000000000000000000000000000002540be40000000000000000000000000000000000000000000000000000000000000000600000000000000000000000000000000000000000000000000000000000000006312E33392E310000000000000000000000000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(withdrawal_signature_encoder_test) {
   encoded_sign_transaction transaction;
   transaction.data = "0xe088747b0000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc1200000000000000000000000000000000000000000000000000000002540be40000000000000000000000000000000000000000000000000000000000000000600000000000000000000000000000000000000000000000000000000000000006312E33392E300000000000000000000000000000000000000000000000000000";
   transaction.sign = sign_hash(keccak_hash(transaction.data), "0x21", "eb5749a569e6141a3b08249d4a0d84f9ef22c67651ba29adb8eb6fd43fc83060" );

   const auto function_signature = signature_encoder::get_function_signature_from_transaction(transaction.data);
   BOOST_REQUIRE_EQUAL(function_signature.empty(), false);
   const signature_encoder encoder{function_signature};
   const auto tx = encoder.encode({transaction});
   BOOST_CHECK_EQUAL(tx, "0xdaac6c810000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000000000000000000080000000000000000000000000000000000000000000000000000000000000006689c3a93d7059430d19ff952900dfada310c0dcced9ed046c335f886091c7e50c1a01016a488777b41a1815ca01a7d809ed47c36dcb0d5f86a43b079ce0d04afe00000000000000000000000000000000000000000000000000000000000000a4e088747b0000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc1200000000000000000000000000000000000000000000000000000002540be40000000000000000000000000000000000000000000000000000000000000000600000000000000000000000000000000000000000000000000000000000000006312E33392E30000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(withdrawal_erc20_encoder_test) {
   const auto tx = withdrawal_erc20_encoder::encode("cc806da9df9d634b5dac0aa36dca1e7780e42C60", "5fbbb31be52608d2f52247e8400b7fcaa9e0bc12", 10, "1.39.0");
   BOOST_CHECK_EQUAL(tx, "0x483c0467000000000000000000000000cc806da9df9d634b5dac0aa36dca1e7780e42C600000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc12000000000000000000000000000000000000000000000000000000000000000a00000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000006312E33392E300000000000000000000000000000000000000000000000000000");

   const auto tx1 = withdrawal_erc20_encoder::encode("cc806da9df9d634b5dac0aa36dca1e7780e42C60", "5fbbb31be52608d2f52247e8400b7fcaa9e0bc12", 10, "1.39.1");
   BOOST_CHECK_EQUAL(tx1, "0x483c0467000000000000000000000000cc806da9df9d634b5dac0aa36dca1e7780e42C600000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc12000000000000000000000000000000000000000000000000000000000000000a00000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000006312E33392E310000000000000000000000000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(withdrawal_erc20_signature_encoder_test) {
   encoded_sign_transaction transaction;
   transaction.data = "0x483c0467000000000000000000000000cc806da9df9d634b5dac0aa36dca1e7780e42C600000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc12000000000000000000000000000000000000000000000000000000000000000a00000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000006312E33392E300000000000000000000000000000000000000000000000000000";
   transaction.sign = sign_hash(keccak_hash(transaction.data), "0x21", "eb5749a569e6141a3b08249d4a0d84f9ef22c67651ba29adb8eb6fd43fc83060" );

   const auto function_signature = signature_encoder::get_function_signature_from_transaction(transaction.data);
   BOOST_REQUIRE_EQUAL(function_signature.empty(), false);
   const signature_encoder encoder{function_signature};
   const auto tx = encoder.encode({transaction});
   BOOST_CHECK_EQUAL(tx, "0xd2bf286600000000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000000000000000000001000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000066f51f7732435016936f0e21aa3c290023ea96ddbc369a957aca28a865cb5004a46675855fccd4bd5a283e1ff61aa60ca9b8b63664e770689e5cfc1a0c6bbdc79a00000000000000000000000000000000000000000000000000000000000000c4483c0467000000000000000000000000cc806da9df9d634b5dac0aa36dca1e7780e42C600000000000000000000000005fbbb31be52608d2f52247e8400b7fcaa9e0bc12000000000000000000000000000000000000000000000000000000000000000a00000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000006312E33392E30000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(update_owners_encoder_test) {
   std::vector<std::pair<std::string, uint16_t>> owners_weights;
   owners_weights.emplace_back("5FbBb31BE52608D2F52247E8400B7fCaA9E0bC12", 1);
   owners_weights.emplace_back("76ce31bd03f601c3fc13732def921c5bac282676", 1);

   const auto tx = update_owners_encoder::encode(owners_weights, "1.39.0");
   BOOST_CHECK_EQUAL(tx, "0x23ab6adf000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000e000000000000000000000000000000000000000000000000000000000000000020000000000000000000000005FbBb31BE52608D2F52247E8400B7fCaA9E0bC12000000000000000000000000000000000000000000000000000000000000000100000000000000000000000076ce31bd03f601c3fc13732def921c5bac28267600000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000006312E33392E300000000000000000000000000000000000000000000000000000");

   owners_weights.emplace_back("09ee460834498a4ee361beb819470061b7381b49", 1);
   const auto tx1 = update_owners_encoder::encode(owners_weights, "1.39.1");
   BOOST_CHECK_EQUAL(tx1, "0x23ab6adf0000000000000000000000000000000000000000000000000000000000000040000000000000000000000000000000000000000000000000000000000000012000000000000000000000000000000000000000000000000000000000000000030000000000000000000000005FbBb31BE52608D2F52247E8400B7fCaA9E0bC12000000000000000000000000000000000000000000000000000000000000000100000000000000000000000076ce31bd03f601c3fc13732def921c5bac282676000000000000000000000000000000000000000000000000000000000000000100000000000000000000000009ee460834498a4ee361beb819470061b7381b4900000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000006312E33392E310000000000000000000000000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(update_owners_signature_encoder_test) {
   encoded_sign_transaction transaction;
   transaction.data = "0x23ab6adf000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000e000000000000000000000000000000000000000000000000000000000000000020000000000000000000000005FbBb31BE52608D2F52247E8400B7fCaA9E0bC12000000000000000000000000000000000000000000000000000000000000000100000000000000000000000076ce31bd03f601c3fc13732def921c5bac28267600000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000006312E33392E300000000000000000000000000000000000000000000000000000";
   transaction.sign = sign_hash(keccak_hash(transaction.data), "0x21", "eb5749a569e6141a3b08249d4a0d84f9ef22c67651ba29adb8eb6fd43fc83060" );

   const auto function_signature = signature_encoder::get_function_signature_from_transaction(transaction.data);
   BOOST_REQUIRE_EQUAL(function_signature.empty(), false);
   const signature_encoder encoder{function_signature};
   const auto tx = encoder.encode({transaction});
   BOOST_CHECK_EQUAL(tx, "0x9d608673000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000008000000000000000000000000000000000000000000000000000000000000000667121da3c6ab5054b1a77cac477f24e7ce7bfcb5e3a857cfcaf48e67fc8f003ac38dfa8821525383608a68c9f215f9a2a232e192ae80079cd2f31b0e01caa6e1d000000000000000000000000000000000000000000000000000000000000012423ab6adf000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000e000000000000000000000000000000000000000000000000000000000000000020000000000000000000000005FbBb31BE52608D2F52247E8400B7fCaA9E0bC12000000000000000000000000000000000000000000000000000000000000000100000000000000000000000076ce31bd03f601c3fc13732def921c5bac28267600000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000006312E33392E30000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(deposit_erc20_decoder_test) {
   const auto erc_20_1 = deposit_erc20_decoder::decode("0x97feb926000000000000000000000000cc806da9df9d634b5dac0aa36dca1e7780e42c600000000000000000000000000000000000000000000000000000000000000064");
   BOOST_REQUIRE_EQUAL(erc_20_1.valid(), true);
   BOOST_CHECK_EQUAL(erc_20_1->token, "0xcc806da9df9d634b5dac0aa36dca1e7780e42c60");
   BOOST_CHECK_EQUAL(erc_20_1->amount, 100);

   const auto erc_20_2 = deposit_erc20_decoder::decode("0x97feb926000000000000000000000000cc806da9df9d634b5dac0aa36dca1e7780e42c600000000000000000000000000000000000000000000000006400000000000000");
   BOOST_REQUIRE_EQUAL(erc_20_2.valid(), true);
   BOOST_CHECK_EQUAL(erc_20_2->token, "0xcc806da9df9d634b5dac0aa36dca1e7780e42c60");
   BOOST_CHECK_EQUAL(erc_20_2->amount, 7205759403792793600);
}

BOOST_AUTO_TEST_CASE(raw_transaction_serialization_test) {
   raw_transaction raw_tr;
   raw_tr.nonce = "0x0";
   raw_tr.gas_price = "0x3b9aca07";
   raw_tr.gas_limit = "0x7a1200";
   raw_tr.to = "0x875a7e0eFe5140c80C5c822f99C02281C0290348";
   raw_tr.value = "";
   raw_tr.data = "";
   raw_tr.chain_id = "0x21";

   const auto tx = raw_tr.serialize();
   BOOST_CHECK_EQUAL(tx, "0xE480843B9ACA07837A120094875A7E0EFE5140C80C5C822F99C02281C02903488080218080");

   //! Change value
   raw_tr.value = "0x1BC16D674EC80000";
   const auto tx1 = raw_tr.serialize();
   BOOST_CHECK_EQUAL(tx1, "0xEC80843B9ACA07837A120094875A7E0EFE5140C80C5C822F99C02281C0290348881BC16D674EC8000080218080");

   //! Change data
   raw_tr.data = "0x893d20e8";
   const auto tx2 = raw_tr.serialize();
   BOOST_CHECK_EQUAL(tx2, "0xF080843B9ACA07837A120094875A7E0EFE5140C80C5C822F99C02281C0290348881BC16D674EC8000084893D20E8218080");
}

BOOST_AUTO_TEST_CASE(raw_transaction_deserialization_test) {
   const raw_transaction raw_tr{"E480843B9ACA07837A120094875A7E0EFE5140C80C5C822F99C02281C02903488080218080"};

   BOOST_CHECK_EQUAL(raw_tr.nonce, "0x0");
   BOOST_CHECK_EQUAL(raw_tr.gas_price, "0x3b9aca07");
   BOOST_CHECK_EQUAL(raw_tr.gas_limit, "0x7a1200");
   BOOST_CHECK_EQUAL(raw_tr.to, "0x875a7e0efe5140c80c5c822f99c02281c0290348");
   BOOST_CHECK_EQUAL(raw_tr.value, "0x0");
   BOOST_CHECK_EQUAL(raw_tr.data, "");
   BOOST_CHECK_EQUAL(raw_tr.chain_id, "0x21");
}

BOOST_AUTO_TEST_CASE(raw_transaction_hash_test) {
   raw_transaction raw_tr;
   raw_tr.nonce = "0x0";
   raw_tr.gas_price = "0x3b9aca07";
   raw_tr.gas_limit = "0x7a1200";
   raw_tr.to = "0x875a7e0eFe5140c80C5c822f99C02281C0290348";
   raw_tr.value = "";
   raw_tr.data = "";
   raw_tr.chain_id = "0x21";

   const auto tx = raw_tr.serialize();
   BOOST_CHECK_EQUAL(tx, "0xE480843B9ACA07837A120094875A7E0EFE5140C80C5C822F99C02281C02903488080218080");

   const auto hash = raw_tr.hash();
   const auto hash_str = fc::to_hex((char *)&hash[0], hash.size());
   BOOST_CHECK_EQUAL(hash_str, "34934410cd305f4fa4e75a2c9294d625d6fbba729b5642ed2ca757ead50bb1fb");
}

BOOST_AUTO_TEST_CASE(sign_transaction_test) {
   raw_transaction raw_tr;
   raw_tr.nonce = "0x0";
   raw_tr.gas_price = "0x3b9aca07";
   raw_tr.gas_limit = "0x7a1200";
   raw_tr.to = "0x875a7e0eFe5140c80C5c822f99C02281C0290348";
   raw_tr.value = "";
   raw_tr.data = "";
   raw_tr.chain_id = "0x21";

   const auto sign_tr = raw_tr.sign("eb5749a569e6141a3b08249d4a0d84f9ef22c67651ba29adb8eb6fd43fc83060");
   BOOST_CHECK_EQUAL(sign_tr.r, "5f09de6ac850b2a9e94acd709c12d4e9adbabc6b72281ec0bbe13bca7e57c7ce");
   BOOST_CHECK_EQUAL(sign_tr.v, "65");
   BOOST_CHECK_EQUAL(sign_tr.s, "7ca5f26c5b3e25f14a32b18ac9a2a41b7c68efd3b04b118e1b1f4bf1c4e299b0");
}

BOOST_AUTO_TEST_CASE(sign_transaction_serialization_test) {
   raw_transaction raw_tr;
   raw_tr.nonce = "0x0";
   raw_tr.gas_price = "0x3b9aca07";
   raw_tr.gas_limit = "0x7a1200";
   raw_tr.to = "0x875a7e0eFe5140c80C5c822f99C02281C0290348";
   raw_tr.value = "";
   raw_tr.data = "";
   raw_tr.chain_id = "0x21";

   const auto sign_tr = raw_tr.sign("eb5749a569e6141a3b08249d4a0d84f9ef22c67651ba29adb8eb6fd43fc83060");
   const auto tx = sign_tr.serialize();
   BOOST_CHECK_EQUAL(tx, "0xF86480843B9ACA07837A120094875A7E0EFE5140C80C5C822F99C02281C0290348808065A05F09DE6AC850B2A9E94ACD709C12D4E9ADBABC6B72281EC0BBE13BCA7E57C7CEA07CA5F26C5B3E25F14A32B18AC9A2A41B7C68EFD3B04B118E1B1F4BF1C4E299B0");
}

BOOST_AUTO_TEST_CASE(sign_transaction_deserialization_test) {
   const signed_transaction sign_tr{"0xF86480843B9ACA07837A120094875A7E0EFE5140C80C5C822F99C02281C0290348808065A05F09DE6AC850B2A9E94ACD709C12D4E9ADBABC6B72281EC0BBE13BCA7E57C7CEA07CA5F26C5B3E25F14A32B18AC9A2A41B7C68EFD3B04B118E1B1F4BF1C4E299B0"};

   BOOST_CHECK_EQUAL(sign_tr.nonce, "0x0");
   BOOST_CHECK_EQUAL(sign_tr.gas_price, "0x3b9aca07");
   BOOST_CHECK_EQUAL(sign_tr.gas_limit, "0x7a1200");
   BOOST_CHECK_EQUAL(sign_tr.to, "0x875a7e0efe5140c80c5c822f99c02281c0290348");
   BOOST_CHECK_EQUAL(sign_tr.value, "0x0");
   BOOST_CHECK_EQUAL(sign_tr.data, "");
}

BOOST_AUTO_TEST_CASE(sign_transaction_recover_test) {
   const std::string chain_id = "0x21";

   raw_transaction raw_tr;
   raw_tr.nonce = "0x0";
   raw_tr.gas_price = "0x3b9aca07";
   raw_tr.gas_limit = "0x7a1200";
   raw_tr.to = "0x875a7e0eFe5140c80C5c822f99C02281C0290348";
   raw_tr.value = "";
   raw_tr.data = "";
   raw_tr.chain_id = chain_id;

   const auto sign_tr = raw_tr.sign("eb5749a569e6141a3b08249d4a0d84f9ef22c67651ba29adb8eb6fd43fc83060");
   const auto from = sign_tr.recover(chain_id);
   BOOST_CHECK_EQUAL(from, "0x5fbbb31be52608d2f52247e8400b7fcaa9e0bc12");
}

BOOST_AUTO_TEST_SUITE_END()