#include <fstream>
#include <filesystem>
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
        ASSERT_TRUE(std::filesystem::exists(OTAP_UPLOAD_FILE_PATH));
    }

protected:
    inline static const std::filesystem::path OTAP_UPLOAD_FILE_PATH = "otap_files/dummy.otap";

    std::vector<uint8_t> ReadUploadFile() const
    {
        const auto size = std::filesystem::file_size(OTAP_UPLOAD_FILE_PATH);
        std::vector<uint8_t> buffer(size);

        if (std::ifstream ifs(OTAP_UPLOAD_FILE_PATH, std::ios_base::binary); ifs) {
            if (ifs.read((char*)buffer.data(), size)) {
                return buffer;
            }
        }

        ADD_FAILURE() << "Cannot open file " << OTAP_UPLOAD_FILE_PATH << ". "
                      << "Please update OTAP_UPLOAD_FILE_PATH to a valid OTAP image";
        return {};
    }

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
    const auto buffer = ReadUploadFile();
    ASSERT_NO_FATAL_FAILURE(UploadScratchpadAndVerify(buffer, 101));

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
    const auto upload_buffer = ReadUploadFile();

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
    auto upload_buffer = ReadUploadFile();
    ASSERT_NO_FATAL_FAILURE(UploadScratchpadAndVerify(upload_buffer, 201));

    std::vector<uint8_t> download_buffer(upload_buffer.size());
    const app_res_e res = WPC_download_local_scratchpad(download_buffer.size(), (uint8_t*)download_buffer.data(), 0);
    ASSERT_NE(APP_RES_ACCESS_DENIED, res) << "Access denied when downloading scratchpad block. "
                                             "Make sure the dual MCU application was built with scratchpad "
                                             "reading enabled.";
    ASSERT_EQ(APP_RES_OK, res);

    ASSERT_NO_FATAL_FAILURE(VerifyScratchpadsAreTheSame(upload_buffer, download_buffer));
}

