/*
 * GridTools
 *
 * Copyright (c) 2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INCLUDED_COMMUNICATION_OBJECT_HPP
#define INCLUDED_COMMUNICATION_OBJECT_HPP

#include <vector>
#include <tuple>
#include <iostream>
#include "gridtools_arch.hpp"
#include "utils.hpp"


namespace gridtools {

    template <typename Pattern, typename Arch>
    class communication_object {};

    template <typename Pattern>
    class communication_object<Pattern, gridtools::cpu> {

        using byte_t = unsigned char;
        using extended_domain_id_t = typename Pattern::extended_domain_id_type;
        using iteration_space_t = typename Pattern::iteration_space2;
        using map_t = typename Pattern::map_type;
        using communicator_t = typename Pattern::communicator_type;
        using future_t = typename communicator_t::template future<void>;
        using s_buffer_t = std::vector<byte_t>;
        using r_buffer_t = std::vector<byte_t>;
        using s_request_t = future_t;
        using r_request_t = std::tuple<std::size_t, extended_domain_id_t, future_t>;

        const Pattern& m_pattern;
        const map_t& m_send_halos;
        const map_t& m_receive_halos;
        std::size_t m_n_send_halos;
        std::size_t m_n_receive_halos;
        std::vector<s_buffer_t> m_send_buffers;
        std::vector<r_buffer_t> m_receive_buffers;
        const communicator_t& m_communicator;

        template <typename... DataDescriptor>
        std::size_t buffer_size(const std::vector<iteration_space_t>& iteration_spaces,
                                const std::tuple<DataDescriptor...>& data_descriptors) {

            std::size_t size{0};

            for (const auto& is : iteration_spaces) {
                gridtools::detail::for_each(data_descriptors, [&is, &size](const auto& dd) {
                    size += is.size() * dd.data_type_size();
                });
            }

            return size;

        }

        template <typename... DataDescriptor>
        void pack(const std::tuple<DataDescriptor...>& data_descriptors) {

            std::size_t halo_index{0};
            for (const auto& halo : m_send_halos) {

                const auto& iteration_spaces = halo.second;

                m_send_buffers[halo_index].resize(buffer_size(iteration_spaces, data_descriptors));
                std::size_t buffer_index{0};

                /* The two loops are performed with this order
                 * in order to have as many data of the same type as possible in contiguos memory */
                gridtools::detail::for_each(data_descriptors, [this, &iteration_spaces, &halo_index, &buffer_index](const auto& dd) {
                    for (const auto& is : iteration_spaces) {
                        dd.get(is, &m_send_buffers[halo_index][buffer_index]);
                        buffer_index += is.size() * dd.data_type_size();
                    }
                });

                ++halo_index;

            }

        }

    public:

        template <typename... DataDescriptor>
        class handle {

            const map_t& m_receive_halos;
            std::vector<r_buffer_t> m_receive_buffers;
            std::vector<r_request_t> m_receive_requests;
            std::tuple<DataDescriptor...> m_data_descriptors;

            void unpack(const std::size_t halo_index, const extended_domain_id_t& domain) {

                const auto& iteration_spaces = m_receive_halos.at(domain);

                std::size_t buffer_index{0};

                /* The two loops are performed with this order
                 * in order to have as many data of the same type as possible in contiguos memory */
                gridtools::detail::for_each(m_data_descriptors, [this, &halo_index, &iteration_spaces, &buffer_index](auto& dd) {
                    for (const auto& is : iteration_spaces) {
                        dd.set(is, &m_receive_buffers[halo_index][buffer_index]);
                        buffer_index += is.size() * dd.data_type_size();
                    }
                });

            }

        public:

            handle(const map_t& receive_halos,
                   const std::vector<r_buffer_t>& receive_buffers,
                   std::vector<r_request_t>&& receive_requests,
                   std::tuple<DataDescriptor...>&& data_descriptors) :
                m_receive_halos{receive_halos},
                m_receive_buffers{receive_buffers},
                m_receive_requests{std::move(receive_requests)},
                m_data_descriptors{std::move(data_descriptors)} {}

            void wait() {

                int i{0};
                for (auto& r : m_receive_requests) {
                    std::cout << "DEBUG: waititng for receive request n. " << i++ << " \n";
                    std::cout.flush();
                    std::get<2>(r).wait();
                    unpack(std::get<0>(r), std::get<1>(r));
                }

            }

        };

        communication_object(const Pattern& p) :
            m_pattern{p},
            m_send_halos{m_pattern.send_halos()},
            m_receive_halos{m_pattern.recv_halos()},
            m_n_send_halos{m_send_halos.size()},
            m_n_receive_halos(m_receive_halos.size()),
            m_send_buffers{m_n_send_halos},
            m_receive_buffers{m_n_receive_halos},
            m_communicator{m_pattern.communicator()} {}

        template <typename... DataDescriptor>
        handle<DataDescriptor...> exchange(DataDescriptor& ...dds) {

            std::vector<s_request_t> send_requests;
            send_requests.reserve(m_n_send_halos); // no default constructor

            std::vector<r_request_t> receive_requests;
            receive_requests.reserve(m_n_receive_halos); // no default constructor

            auto data_descriptors = std::make_tuple(dds...);

            std::size_t halo_index;

            /* RECEIVE */

            halo_index = 0;
            for (const auto& halo : m_receive_halos) {

                auto domain = halo.first;
                auto source = domain.address;
                auto tag = domain.tag;
                const auto& iteration_spaces = halo.second;

                m_receive_buffers[halo_index].resize(buffer_size(iteration_spaces, data_descriptors));

                std::cout << "DEBUG: Starting receive request for halo_index = " << halo_index << " \n";
                std::cout.flush();

                receive_requests.push_back(std::make_tuple(halo_index, domain, m_communicator.irecv(
                                                              source,
                                                              tag,
                                                              &m_receive_buffers[halo_index][0],
                                                              static_cast<int>(m_receive_buffers[halo_index].size()))));

                ++halo_index;

            }

            /* SEND */

            pack(data_descriptors);

            halo_index = 0;
            for (const auto& halo : m_send_halos) {

                auto dest = halo.first.address;
                auto tag = halo.first.tag;

                std::cout << "DEBUG: Starting send request for halo_index = " << halo_index << " \n";
                std::cout.flush();

                send_requests.push_back(m_communicator.isend(dest,
                        tag,
                        &m_send_buffers[halo_index][0],
                        static_cast<int>(m_send_buffers[halo_index].size())));

                ++halo_index;

            }

            /* SEND WAIT */

            int i{0};
            for (auto& r : send_requests) {
                std::cout << "DEBUG: waititng for send request n. " << i++ << " \n";
                std::cout.flush();
                r.wait();
            }

            return {m_receive_halos, m_receive_buffers, std::move(receive_requests), std::move(data_descriptors)};

        }

    };

}

#endif /* INCLUDED_COMMUNICATION_OBJECT_HPP */
