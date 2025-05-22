#include "stdafx.h"

#include "gtest/gtest.h"

#include "Files/filemanager.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(universal_file_accessor_test, open_file_in_exe_directory)
        {
            auto file = UniversalFileAccessor::openTextFile("testfile.txt");

            EXPECT_EQ(file.filepath, "testfile.txt");
            EXPECT_EQ(file.data, "This is a test file !");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(universal_file_accessor_test, file_info_in_exe_directory)
        {
            MockLogger logger;

            auto file = UniversalFileAccessor::openTextFile("testfile.txt");

            EXPECT_EQ(file.filepath, "testfile.txt");
            EXPECT_EQ(file.data, "This is a test file !");

            auto filename = UniversalFileAccessor::getFileName(file);
            auto folderpath = UniversalFileAccessor::getFoldername(file);

            EXPECT_EQ(filename, "testfile.txt");
            EXPECT_EQ(folderpath, "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(universal_file_accessor_test, open_file_in_exe_sub_directory)
        {
            auto file = UniversalFileAccessor::openTextFile("testtextfolder/file1.txt");

            EXPECT_EQ(file.filepath, "testtextfolder/file1.txt");
            EXPECT_EQ(file.data, "This is the first file !");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(universal_file_accessor_test, file_info_in_exe_sub_directory)
        {
            MockLogger logger;

            auto file = UniversalFileAccessor::openTextFile("testtextfolder/file1.txt");

            EXPECT_EQ(file.filepath, "testtextfolder/file1.txt");
            EXPECT_EQ(file.data, "This is the first file !");

            auto filename = UniversalFileAccessor::getFileName(file);
            auto folderpath = UniversalFileAccessor::getFoldername(file);

            EXPECT_EQ(filename, "file1.txt");
            EXPECT_EQ(folderpath, "testtextfolder/");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(universal_file_accessor_test, all_files_in_exe_sub_directory)
        {
            MockLogger logger;

            auto folder = UniversalFileAccessor::openTextFolder("testtextfolder/");

            EXPECT_EQ(folder.size(), 3);

            for(const auto& file : folder)
            {
                if(file.filepath == "testtextfolder/file1.txt")
                {
                    EXPECT_EQ(file.filepath, "testtextfolder/file1.txt");
                    EXPECT_EQ(file.data, "This is the first file !");

                    auto filename = UniversalFileAccessor::getFileName(file);
                    auto folderpath = UniversalFileAccessor::getFoldername(file);

                    EXPECT_EQ(filename, "file1.txt");
                    EXPECT_EQ(folderpath, "testtextfolder/");
                }
                else if(file.filepath == "testtextfolder/file2.txt")
                {
                    EXPECT_EQ(file.filepath, "testtextfolder/file2.txt");
                    EXPECT_EQ(file.data, "This is the second file ?");

                    auto filename = UniversalFileAccessor::getFileName(file);
                    auto folderpath = UniversalFileAccessor::getFoldername(file);

                    EXPECT_EQ(filename, "file2.txt");
                    EXPECT_EQ(folderpath, "testtextfolder/");
                }
                else if(file.filepath == "testtextfolder/file3.txt")
                {
                    EXPECT_EQ(file.filepath, "testtextfolder/file3.txt");
                    EXPECT_EQ(file.data, "Good job !");

                    auto filename = UniversalFileAccessor::getFileName(file);
                    auto folderpath = UniversalFileAccessor::getFoldername(file);

                    EXPECT_EQ(filename, "file3.txt");
                    EXPECT_EQ(folderpath, "testtextfolder/");
                }
                else if (file.filepath != "testtextfolder/." and file.filepath != "testtextfolder/..")
                {
                    EXPECT_TRUE(false);
                }
            }


        }

    } // namespace test

} // namespace pg