/*
 * Copyright (c) 2015, 2016, 2017, 2018, 2019, 2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <map>
#include <string>

#include "SSTIO.hpp"
#include <type_traits>

namespace geopm
{


    class SSTIOImp : public SSTIO
    {
        public:
            SSTIOImp(int max_cpus);
            virtual ~SSTIOImp() = default;

            /// Interact with the mailbox on commands that are expected to return data
            int add_mbox_read(uint32_t cpu_index, uint16_t command,
                              uint16_t subcommand, uint32_t subcommand_arg,
                              uint32_t interface_parameter) override;
            int add_mbox_write(uint32_t cpu_index, uint16_t command,
                               uint16_t subcommand, uint32_t interface_parameter,
                               uint16_t read_subcommand,
                               uint32_t read_interface_parameter,
                               uint32_t read_mask) override;
            int add_mmio_read(uint32_t cpu_index, uint16_t register_offset,
                              uint32_t register_value) override;
            int add_mmio_write(uint32_t cpu_index, uint16_t register_offset,
                               uint32_t register_value) override;
            // call ioctl() for both mbox list and mmio list,
            // unless we end up splitting this class
            void read_batch(void) override;

            // TODO: might need separate call for mbox and mmio
            uint64_t sample(int index) const override;
            void write_batch(void) override;
            void adjust(int index, uint64_t write_value, uint64_t write_mask) override;
            uint32_t get_punit_from_cpu(uint32_t cpu_index) override;

        private:
            struct sst_cpu_map_interface_s
            {
                uint32_t cpu_index;
                uint32_t punit_cpu;
            };

            struct sst_cpu_map_interface_batch_s
            {
                uint32_t num_entries;
                sst_cpu_map_interface_s interfaces[1];
            };

            struct sst_mmio_interface_s
            {
                uint32_t is_write;
                uint32_t cpu_index;
                uint32_t register_offset;
                uint32_t value;
            };

            struct sst_mmio_interface_batch_s
            {
                uint32_t num_entries;
                sst_mmio_interface_s interfaces[1];
            };

            struct sst_mbox_interface_s
            {
                uint32_t cpu_index;
                uint32_t mbox_interface_param; // Parameter to the mbox interface itself
                uint32_t write_value; // Mailbox data, or input parameter for a read
                uint32_t read_value; // Mailbox data (read-only)
                uint16_t command;
                uint16_t subcommand;
                uint32_t reserved;
            };

            struct sst_mbox_interface_batch_s
            {
                uint32_t num_entries;
                sst_mbox_interface_s interfaces[1];  // TODO: might need to be array instead
            };

            template<typename ListStruct, typename CommandStruct>
            class CommandList
            {
                public:
                explicit CommandList(size_t command_count);
            };

            template<typename OuterStruct>
            using InnerStruct =
                typename std::remove_all_extents<decltype(OuterStruct::interfaces)>::type;

            template<typename OuterStruct>
            std::unique_ptr<OuterStruct>
                ioctl_struct_from_vector(std::vector<InnerStruct<OuterStruct>> commands)

            {
                std::unique_ptr<OuterStruct> outer_struct(reinterpret_cast<OuterStruct *>(
                    new char[sizeof(OuterStruct::num_entries) +
                             sizeof(InnerStruct<OuterStruct>) *
                                 commands.size()]));

                outer_struct->num_entries = commands.size();
                std::memcpy(outer_struct->interfaces, commands.data(),
                            outer_struct->num_entries *
                                sizeof(InnerStruct<OuterStruct>));

                return outer_struct;
            }

            std::string m_path;
            int m_fd;
            // TODO:
            //  vector<mbox> m_mbox_interface
            //  vector<mmio> m_mmio_interface
            //  vector< pair<which_list, list_idx> > pushed_offsets;
            //   `-- push_xyz() returns an index to this, where the value
            //       at that index indicates either mmio or mbox, and the
            //       offset within that mmio/mbox list
            //
            //
            std::vector<struct sst_mbox_interface_s> m_mbox_read_interfaces;
            std::vector<struct sst_mbox_interface_s> m_mbox_write_interfaces;
            std::vector<struct sst_mbox_interface_s> m_mbox_rmw_interfaces;
            std::vector<uint32_t> m_mbox_rmw_read_masks;
            std::vector<uint32_t> m_mbox_rmw_write_masks;
            std::vector<struct sst_mmio_interface_s> m_mmio_interfaces;
            std::vector<std::pair<bool /*TODO:enum interface type*/, size_t> > m_added_interfaces;
            std::unique_ptr<sst_mbox_interface_batch_s> m_mbox_read_batch;
            std::unique_ptr<sst_mbox_interface_batch_s> m_mbox_write_batch;
            std::unique_ptr<sst_mmio_interface_batch_s> m_mmio_read_batch;
            std::unique_ptr<sst_mmio_interface_batch_s> m_mmio_write_batch;
            std::map<uint32_t, uint32_t> m_cpu_punit_core_map;
    };
}
