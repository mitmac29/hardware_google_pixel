/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "adaptivecpu/CpuFrequencyReader.h"
#include "mocks.h"

using testing::_;
using testing::ByMove;
using testing::Return;
using std::chrono_literals::operator""ms;

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

std::ostream &operator<<(std::ostream &stream, const CpuPolicyAverageFrequency &frequency) {
    return stream << "CpuPolicyAverageFrequency(" << frequency.policyId << ", "
                  << frequency.averageFrequencyHz << ")";
}

TEST(CpuFrequencyReaderTest, cpuPolicyIds) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, listDirectory("/sys/devices/system/cpu/cpufreq"))
            .WillOnce(Return(std::vector<std::string>(
                    {"ignored1", "policy1", "ignored2", "policy5", "policy10", "policybad"})));
    EXPECT_CALL(*filesystem, readFile(_)).WillRepeatedly([]() {
        return std::make_unique<std::istringstream>("1 2\n3 4\n");
    });

    CpuFrequencyReader reader(std::move(filesystem));
    EXPECT_TRUE(reader.init());

    std::map<uint32_t, std::map<uint64_t, std::chrono::milliseconds>> expected = {
            {1, {{1, 20ms}, {3, 40ms}}}, {5, {{1, 20ms}, {3, 40ms}}}, {10, {{1, 20ms}, {3, 40ms}}}};
    EXPECT_EQ(reader.getPreviousCpuPolicyFrequencies(), expected);
}

TEST(CpuFrequencyReaderTest, getRecentCpuPolicyFrequencies) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, listDirectory("/sys/devices/system/cpu/cpufreq"))
            .WillOnce(Return(std::vector<std::string>({"policy1", "policy2"})));
    EXPECT_CALL(*filesystem,
                readFile("/sys/devices/system/cpu/cpufreq/policy1/stats/time_in_state"))
            .Times(2)
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1000 5\n2000 4"))))
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1000 7\n2000 10"))));
    EXPECT_CALL(*filesystem,
                readFile("/sys/devices/system/cpu/cpufreq/policy2/stats/time_in_state"))
            .Times(2)
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1500 1\n2500 23"))))
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1500 5\n2500 23"))));

    CpuFrequencyReader reader(std::move(filesystem));
    EXPECT_TRUE(reader.init());

    std::vector<CpuPolicyAverageFrequency> actual;
    EXPECT_TRUE(reader.getRecentCpuPolicyFrequencies(&actual));
    EXPECT_EQ(actual, std::vector<CpuPolicyAverageFrequency>({
                              {.policyId = 1, .averageFrequencyHz = 1750},
                              {.policyId = 2, .averageFrequencyHz = 1500},
                      }));
}

TEST(CpuFrequencyReaderTest, getRecentCpuPolicyFrequencies_frequenciesChange) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, listDirectory("/sys/devices/system/cpu/cpufreq"))
            .WillOnce(Return(std::vector<std::string>({"policy1"})));
    EXPECT_CALL(*filesystem,
                readFile("/sys/devices/system/cpu/cpufreq/policy1/stats/time_in_state"))
            .Times(2)
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1000 5\n2000 4"))))
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1000 6\n2001 4"))));

    CpuFrequencyReader reader(std::move(filesystem));
    EXPECT_TRUE(reader.init());

    std::vector<CpuPolicyAverageFrequency> actual;
    EXPECT_FALSE(reader.getRecentCpuPolicyFrequencies(&actual));
}

TEST(CpuFrequencyReaderTest, getRecentCpuPolicyFrequencies_badFormat) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, listDirectory("/sys/devices/system/cpu/cpufreq"))
            .WillOnce(Return(std::vector<std::string>({"policy1"})));
    EXPECT_CALL(*filesystem,
                readFile("/sys/devices/system/cpu/cpufreq/policy1/stats/time_in_state"))
            .Times(2)
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1000 1"))))
            .WillOnce(Return(ByMove(std::make_unique<std::istringstream>("1000 2\nfoo"))));

    CpuFrequencyReader reader(std::move(filesystem));
    EXPECT_TRUE(reader.init());

    std::vector<CpuPolicyAverageFrequency> actual;
    EXPECT_FALSE(reader.getRecentCpuPolicyFrequencies(&actual));
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
