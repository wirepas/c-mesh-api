#include <gtest/gtest.h>
#include "wpc_test.hpp"

class WpcScratchpadTest : public WpcTest
{
public:
    static void SetUpTestSuite()
    {
        ASSERT_NO_FATAL_FAILURE(WpcTest::SetUpTestSuite());
        ASSERT_NO_FATAL_FAILURE(WpcTest::StopStack());
        ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));
    }

protected:
    inline static const std::vector<uint8_t> DUMMY_SCRATCHPAD = {
        0x53, 0x43, 0x52, 0x31, 0x9a, 0x93, 0x30, 0x82, 0xd9, 0xeb, 0x0a,
        0xfc, 0x31, 0x21, 0xe3, 0x37, 0xb0, 0x00, 0x00, 0x00, 0x33, 0x6c,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0x53, 0x43, 0x52, 0x31, 0x9a, 0x93, 0x30,
        0x82, 0xd9, 0xeb, 0x0a, 0xfc, 0x31, 0x21, 0xe3, 0x37, 0x80, 0x00,
        0x00, 0x00, 0x0b, 0x27, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
        0xff, 0xff, 0xff, 0xaf, 0x29, 0xd6, 0xc6, 0x09, 0xfd, 0x8f, 0x78,
        0xba, 0xb5, 0xc4, 0x2f, 0x43, 0x6d, 0xf1, 0xcd, 0xd4, 0x6a, 0x0d,
        0xa3, 0x1a, 0x8c, 0xcb, 0xd9, 0xf0, 0xf9, 0xb1, 0xdf, 0x4a, 0xef,
        0x1e, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xa4, 0xae, 0xab, 0x5b,
        0xce, 0x4c, 0x4a, 0x45, 0xda, 0x77, 0xe2, 0x48, 0xd2, 0x03, 0x62,
        0x79, 0xf9, 0x98, 0xc2, 0xa1, 0x0a, 0x8e, 0x15, 0x5b, 0x21, 0x12,
        0xbe, 0x2e, 0x5e, 0x79, 0x16, 0xd4, 0x58, 0xaf, 0x83, 0x74, 0x6e,
        0x9a, 0x7c, 0x0e, 0x26, 0xcd, 0x5d, 0xf6, 0xc7, 0xa5, 0x40, 0x4e,
        0xb9, 0xc3, 0x5c, 0x02, 0x62, 0xab, 0xdb, 0x21, 0xc3, 0x7e, 0x64,
        0xe9, 0xcc, 0x95, 0xb2, 0x34, 0x5e, 0xab, 0xd0, 0x7f, 0x5e, 0xe2,
        0x26, 0x8c, 0x3f, 0x6b, 0xc7, 0xc6, 0x33, 0x04, 0x83, 0x6e
    };

    void UploadScratchpadAndVerify(const std::vector<uint8_t>& buffer, const uint8_t seq_number) const
    {
        ASSERT_FALSE(buffer.empty());

        ASSERT_EQ(APP_RES_OK, WPC_upload_local_scratchpad(buffer.size(), (uint8_t*)buffer.data(), seq_number));

        ASSERT_NO_FATAL_FAILURE(VerifyUploadedScratchpad(seq_number, buffer.size()));
    }

    void VerifyUploadedScratchpad(const uint8_t seq_number, const uint32_t size) const
    {
        app_scratchpad_status_t status;
        ASSERT_EQ(APP_RES_OK, WPC_get_local_scratchpad_status(&status));
        ASSERT_EQ(size, status.scrat_len);
        ASSERT_EQ(seq_number, status.scrat_seq_number);

        uint32_t reported_size = 0;
        ASSERT_EQ(APP_RES_OK, WPC_get_scratchpad_size(&reported_size));
        ASSERT_EQ(size, reported_size);

        uint8_t reported_seq = 0;
        ASSERT_EQ(APP_RES_OK, WPC_get_scratchpad_sequence(&reported_seq));
        ASSERT_EQ(seq_number, reported_seq);
    }

    void VerifyScratchpadsAreTheSame(const std::vector<uint8_t>& first, const std::vector<uint8_t>& second) const
    {
        std::vector<uint8_t> first_copy = { first };
        std::vector<uint8_t> second_copy = { second };
 
        // Scratchpad header (16 bytes starting at index 16) includes information
        // that can be different when downloaded, so exclude that part when
        // doing the comparison.
        ASSERT_GE(first_copy.size(), 32);
        ASSERT_GE(second_copy.size(), 32);
        std::fill_n(first_copy.begin() + 16, 16, 0);
        std::fill_n(second_copy.begin() + 16, 16, 0);

        ASSERT_EQ(first_copy, second_copy);
    }
};

TEST_F(WpcScratchpadTest, testUploadAndClearScratchpad)
{
    ASSERT_NO_FATAL_FAILURE(UploadScratchpadAndVerify(DUMMY_SCRATCHPAD, 101));

    ASSERT_EQ(APP_RES_OK, WPC_clear_local_scratchpad());

    app_scratchpad_status_t status;
    ASSERT_EQ(APP_RES_OK, WPC_get_local_scratchpad_status(&status));
    ASSERT_EQ(0, status.scrat_len);
    ASSERT_EQ(0, status.scrat_seq_number);

    uint32_t size;
    ASSERT_EQ(APP_RES_ATTRIBUTE_NOT_SET, WPC_get_scratchpad_size(&size));

    uint8_t seq;
    ASSERT_EQ(APP_RES_OK, WPC_get_scratchpad_sequence(&seq));
    ASSERT_EQ(0, seq);
}

TEST_F(WpcScratchpadTest, testUploadAndDownloadScratchpadInBlocks)
{
    const auto& upload_buffer = DUMMY_SCRATCHPAD;

    const uint32_t MAX_BLOCK_SIZE = 32;
    ASSERT_LT(MAX_BLOCK_SIZE, upload_buffer.size());

    const uint8_t TEST_SEQ = 99;
    ASSERT_EQ(APP_RES_OK, WPC_start_local_scratchpad_update(upload_buffer.size(), TEST_SEQ));

    for (size_t written = 0, remaining = upload_buffer.size(); remaining > 0;)
    {
        const uint32_t block_size = (remaining > MAX_BLOCK_SIZE) ? MAX_BLOCK_SIZE : remaining;
        const uint8_t* block_ptr = (uint8_t*) upload_buffer.data() + written;

        ASSERT_EQ(APP_RES_OK, WPC_upload_local_block_scratchpad(block_size, block_ptr, written));

        written += block_size;
        remaining -= block_size;
    }

    ASSERT_NO_FATAL_FAILURE(VerifyUploadedScratchpad(TEST_SEQ, upload_buffer.size()));

    std::vector<uint8_t> download_buffer(upload_buffer.size());
    for (size_t read = 0, remaining = upload_buffer.size(); remaining > 0;)
    {
        const uint32_t block_size = (remaining > MAX_BLOCK_SIZE) ? MAX_BLOCK_SIZE : remaining;
        uint8_t* block_ptr = (uint8_t*) download_buffer.data() + read;

        const app_res_e res = WPC_download_local_scratchpad(block_size, block_ptr, read);
        ASSERT_NE(APP_RES_ACCESS_DENIED, res) << "Access denied when downloading scratchpad block. "
                                                 "Make sure the dual MCU application was built with scratchpad "
                                                 "reading enabled.";
        ASSERT_EQ(APP_RES_OK, res);

        read += block_size;
        remaining -= block_size;
    }

    ASSERT_NO_FATAL_FAILURE(VerifyScratchpadsAreTheSame(upload_buffer, download_buffer));
}

TEST_F(WpcScratchpadTest, testUploadAndDownloadScratchpad)
{
    const auto& upload_buffer = DUMMY_SCRATCHPAD;
    ASSERT_NO_FATAL_FAILURE(UploadScratchpadAndVerify(upload_buffer, 201));

    std::vector<uint8_t> download_buffer(upload_buffer.size());
    const app_res_e res = WPC_download_local_scratchpad(download_buffer.size(), (uint8_t*)download_buffer.data(), 0);
    ASSERT_NE(APP_RES_ACCESS_DENIED, res) << "Access denied when downloading scratchpad block. "
                                             "Make sure the dual MCU application was built with scratchpad "
                                             "reading enabled.";
    ASSERT_EQ(APP_RES_OK, res);

    ASSERT_NO_FATAL_FAILURE(VerifyScratchpadsAreTheSame(upload_buffer, download_buffer));
}

