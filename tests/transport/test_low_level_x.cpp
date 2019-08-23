#include <transport_layer/progress.hpp>
#include <transport_layer/mpi/communicator.hpp>
#include <vector>
#include <iomanip>
#include <utility>

#include <gtest/gtest.h>

/**
 * Simple Send recv on two ranks.
 * P0 sends a message to P1 and receive from P1,
 * P1 sends a message to P0 and receive from P0.
 */

int rank;

auto test1() {
    gridtools::ghex::mpi::communicator sr;

    std::vector<unsigned char> smsg = {0,0,0,0,1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0,5,0,0,0,6,0,0,0,7,0,0,0,8,0,0,0,9,0,0,0};
    std::vector<unsigned char> rmsg(40, 40);

    gridtools::ghex::mpi::communicator::future_type rfut;

    if ( rank == 0 ) {
        sr.blocking_send(smsg, 1, 1);
        rfut = sr.recv(rmsg, 1, 2);
    } else if (rank == 1) {
        sr.blocking_send(smsg, 0, 2);
        rfut = sr.recv(rmsg, 0, 1);
    }

#ifdef GHEX_TEST_COUNT_ITERATIONS
    int c = 0;
#endif
    do {
#ifdef GHEX_TEST_COUNT_ITERATIONS
        c++;
#endif
     } while (!rfut.ready());

#ifdef GHEX_TEST_COUNT_ITERATIONS
    std::cout << "\n***********\n";
    std::cout <<   "*" << std::setw(8) << c << " *\n";
    std::cout << "***********\n";
#endif

    return rmsg;
}

auto test2() {
    gridtools::ghex::mpi::communicator sr;
    using allocator_type = std::allocator<unsigned char>;
    using smsg_type      = gridtools::ghex::mpi::shared_message<allocator_type>;
    using comm_type      = std::remove_reference_t<decltype(sr)>;

    gridtools::ghex::progress<comm_type,allocator_type> progress(sr);

    std::vector<unsigned char> smsg = {0,0,0,0,1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0,5,0,0,0,6,0,0,0,7,0,0,0,8,0,0,0,9,0,0,0};
    smsg_type rmsg(40, 40);

    bool arrived = false;

    if ( rank == 0 ) {
        auto fut = sr.send(smsg, 1, 1);
        progress.recv(rmsg, 1, 2, [ &arrived,&rmsg](int, int, const smsg_type&) { arrived = true; });
        fut.wait();
    } else if (rank == 1) {
        auto fut = sr.send(smsg, 0, 2);
        progress.recv(rmsg, 0, 1, [ &arrived,&rmsg](int, int, const smsg_type&) { arrived = true; });
        fut.wait();
    }

#ifdef GHEX_TEST_COUNT_ITERATIONS
    int c = 0;
#endif
    do {
#ifdef GHEX_TEST_COUNT_ITERATIONS
        c++;
#endif
        progress();
     } while (!arrived);

#ifdef GHEX_TEST_COUNT_ITERATIONS
    std::cout << "\n***********\n";
    std::cout <<   "*" << std::setw(8) << c << " *\n";
    std::cout << "***********\n";
#endif

    EXPECT_FALSE(progress());

    return rmsg;
}

auto test1_mesg() {
    gridtools::ghex::mpi::communicator sr;

    gridtools::ghex::mpi::message<> smsg{40, 40};

    int* data = smsg.data<int>();

    for (int i = 0; i < 10; ++i) {
        data[i] = i;
    }

    gridtools::ghex::mpi::message<> rmsg{40, 40};

    gridtools::ghex::mpi::communicator::future_type rfut;

    if ( rank == 0 ) {
        sr.blocking_send(smsg, 1, 1);
        rfut = sr.recv(rmsg, 1, 2);
    } else if (rank == 1) {
        sr.blocking_send(smsg, 0, 2);
        rfut = sr.recv(rmsg, 0, 1);
    }

#ifdef GHEX_TEST_COUNT_ITERATIONS
    int c = 0;
#endif
    do {
#ifdef GHEX_TEST_COUNT_ITERATIONS
        c++;
#endif
    } while (!rfut.ready());

#ifdef GHEX_TEST_COUNT_ITERATIONS
    std::cout << "\n***********\n";
    std::cout <<   "*" << std::setw(8) << c << " *\n";
    std::cout << "***********\n";
#endif


    return rmsg;
}

auto test2_mesg() {
    gridtools::ghex::mpi::communicator sr;
    using allocator_type = std::allocator<unsigned char>;
    using smsg_type      = gridtools::ghex::mpi::shared_message<allocator_type>;
    using comm_type      = std::remove_reference_t<decltype(sr)>;

    gridtools::ghex::progress<comm_type,allocator_type> progress(sr);

    gridtools::ghex::mpi::message<> smsg{40, 40};

    int* data = smsg.data<int>();

    for (int i = 0; i < 10; ++i) {
        data[i] = i;
    }

    smsg_type rmsg{40, 40};

    bool arrived = false;

    if ( rank == 0 ) {
        auto fut = sr.send(smsg, 1, 1);
        progress.recv(rmsg, 1, 2, [ &arrived](int, int, const smsg_type&) { arrived = true; });
        fut.wait();
    } else if (rank == 1) {
        auto fut = sr.send(smsg, 0, 2);
        progress.recv(rmsg, 0, 1, [ &arrived](int, int, const smsg_type&) { arrived = true; });
        fut.wait();
    }

#ifdef GHEX_TEST_COUNT_ITERATIONS
    int c = 0;
#endif
    do {
#ifdef GHEX_TEST_COUNT_ITERATIONS
        c++;
#endif
        progress();
     } while (!arrived);

#ifdef GHEX_TEST_COUNT_ITERATIONS
    std::cout << "\n***********\n";
    std::cout <<   "*" << std::setw(8) << c << " *\n";
    std::cout << "***********\n";
#endif

    EXPECT_FALSE(progress());


    return rmsg;
}

auto test1_shared_mesg() {
    gridtools::ghex::mpi::communicator sr;

    gridtools::ghex::mpi::shared_message<> smsg{40, 40};
    int* data = smsg.data<int>();

    for (int i = 0; i < 10; ++i) {
        data[i] = i;
    }

    gridtools::ghex::mpi::shared_message<> rmsg{40, 40};

    gridtools::ghex::mpi::communicator::future_type rfut;

    if ( rank == 0 ) {
        auto sf = sr.send(smsg, 1, 1);
        rfut = sr.recv(rmsg, 1, 2);
        sf.wait();
    } else if (rank == 1) {
        auto sf = sr.send(smsg, 0, 2);
        rfut = sr.recv(rmsg, 0, 1);
        sf.wait();
    }

#ifdef GHEX_TEST_COUNT_ITERATIONS
    int c = 0;
#endif
    do {
#ifdef GHEX_TEST_COUNT_ITERATIONS
        c++;
#endif
     } while (!rfut.ready());

#ifdef GHEX_TEST_COUNT_ITERATIONS
    std::cout << "\n***********\n";
    std::cout <<   "*" << std::setw(8) << c << " *\n";
    std::cout << "***********\n";
#endif

    return rmsg;
}


template <typename M>
bool check_msg(M const& msg) {
    bool ok = true;
    if (rank > 1)
        return ok;

    int* data = msg.template data<int>();
    for (size_t i = 0; i < msg.size()/sizeof(int); ++i) {
        if ( data[i] != static_cast<int>(i) )
            ok = false;
    }
    return ok;
}

bool check_msg(std::vector<unsigned char> const& msg) {
    bool ok = true;
    if (rank > 1)
        return ok;

    int c = 0;
    for (size_t i = 0; i < msg.size(); i += 4) {
        int value = *(reinterpret_cast<int const*>(&msg[i]));
        if ( value != c++ )
            ok = false;
    }
    return ok;
}

template <typename Test>
bool run_test(Test&& test) {
    bool ok;
    auto msg = test();


    ok = check_msg(msg);
    return ok;
}


TEST(low_level, basic_x) {

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank < 2) {
        EXPECT_TRUE(run_test(test1));
    }
}

TEST(low_level, basic_x_call_back) {

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank < 2) {
        EXPECT_TRUE(run_test(test2));
    }
}

TEST(low_level, basic_x_msg) {

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank < 2) {
        EXPECT_TRUE(run_test(test1_mesg));
    }
}

TEST(low_level, basic_x_msg_call_back) {

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank < 2) {
        EXPECT_TRUE(run_test(test2_mesg));
    }
}

TEST(low_level, basic_x_shared_msg) {

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank < 2) {
        EXPECT_TRUE(run_test(test1_shared_mesg));
    }
}

