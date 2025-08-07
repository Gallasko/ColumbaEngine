#include "stdafx.h"

#include <iostream>

#include "gtest/gtest.h"

#include "taskflow/taskflow.hpp"
#include "taskflow/algorithm/pipeline.hpp"

namespace
{
    int spawn(int n, tf::Subflow& sbf)
    {
        if (n < 2) return n;
        int res1, res2;

        // compute f(n-1)
        sbf.emplace([&res1, n] (tf::Subflow& sbf) { res1 = spawn(n - 1, sbf); } )
            .name(std::to_string(n-1));

        // compute f(n-2)
        sbf.emplace([&res2, n] (tf::Subflow& sbf) { res2 = spawn(n - 2, sbf); } )
            .name(std::to_string(n-2));

        sbf.join();
        return res1 + res2;
    }

    struct MyObserver : public tf::ObserverInterface
    {
        MyObserver(const std::string&) {
            // std::cout << "constructing observer " << name << '\n';
        }

        // set_up is a constructor-like method that will be called exactly once
        // passing the number of workers
        void set_up(size_t) override final {
            // std::cout << "setting up observer with " << num_workers << " workers\n";
        }

        // on_entry will be called before a worker runs a task
        void on_entry(tf::WorkerView, tf::TaskView) override final {
            // std::ostringstream oss;
            // oss << "worker " << wv.id() << " ready to run " << tv.name() << '\n';
            // std::cout << oss.str();
        }

        // on_exit will be called after a worker completes a task
        void on_exit(tf::WorkerView, tf::TaskView) override final {
            // std::ostringstream oss;
            // oss << "worker " << wv.id() << " finished running " << tv.name() << '\n';
            // std::cout << oss.str();
        }
    };
}

// ----------------------------------------------------------------------------------------
// ---------------------------        Test separator        -------------------------------
// ----------------------------------------------------------------------------------------
TEST(taskflow_test, fibonacci)
{
    constexpr unsigned int N = 15;

    int res;  // result

    tf::Executor executor;
    tf::Taskflow taskflow("fibonacci");

    taskflow.emplace([&res, N] (tf::Subflow& sbf) {
        res = spawn(N, sbf);
    }).name(std::to_string(N));

    executor.run(taskflow).wait();

    EXPECT_EQ(res, 610);

    //taskflow.dump(std::cout);
}

// ----------------------------------------------------------------------------------------
// ---------------------------        Test separator        -------------------------------
// ----------------------------------------------------------------------------------------
TEST(taskflow_test, composition)
{
    tf::Executor executor;

    float x = 0, y = 0, res = 0;

    // f1 has two independent tasks
    tf::Taskflow f1("F1");
    auto f1A = f1.emplace([&](){ x += 1; });
    auto f1B = f1.emplace([&](){ y -= 1; });
    f1A.name("f1A");
    f1B.name("f1B");

    //  f2A ---
    //         |----> f2C
    //  f2B ---
    //
    //  f1_module_task
    tf::Taskflow f2("F2");
    auto f2A = f2.emplace([&](){ x *= 2; });
    auto f2B = f2.emplace([&](){ y /= 2; });
    auto f2C = f2.emplace([&](){ res = x + y; });
    f2A.name("f2A");
    f2B.name("f2B");
    f2C.name("f2C");

    f2A.precede(f2C);
    f2B.precede(f2C);
    f2.composed_of(f1).name("module_of_f1");

    // f3 has a module task (f2) and a regular task
    tf::Taskflow f3("F3");
    f3.composed_of(f2).name("module_of_f2");
    f3.emplace([](){ EXPECT_TRUE(true); }).name("f3A");

    // f4: f3_module_task -> f2_module_task
    tf::Taskflow f4;
    f4.name("F4");
    auto f3_module_task = f4.composed_of(f3).name("module_of_f3");
    auto f2_module_task = f4.composed_of(f2).name("module_of_f2");
    f3_module_task.precede(f2_module_task);

    // f4.dump(std::cout);

    executor.run_until(
        f4,
        [iter = 1] () mutable { return iter-- == 0; },
        [=](){ }
    ).get();

    std::cout << "Potential values after one interation: " << "x: " << x << ", y: " << y << ", res: " << res << std::endl;

    x = 0; y = 0; res = 0;

    executor.run_until(
        f4,
        [iter = 2] () mutable { return iter-- == 0; },
        [=](){ }
    );

    std::cout << "Potential values after two interation: " << "x: " << x << ", y: " << y << ", res: " << res << std::endl;

    x = 0; y = 0; res = 0;

    executor.run_until(
        f4,
        [iter = 3] () mutable { return iter-- == 0; },
        [=](){ }
    ).get();

    std::cout << "Potential values after three interation: " << "x: " << x << ", y: " << y << ", res: " << res << std::endl;
}

// ----------------------------------------------------------------------------------------
// ---------------------------        Test separator        -------------------------------
// ----------------------------------------------------------------------------------------
TEST(taskflow_test, personal_composition)
{
    tf::Executor executor;

    std::string resGraph = "";
    std::mutex mtx;

    // f1 has two independent tasks
    tf::Taskflow f1("F1");
    auto f1A = f1.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "F1 TaskA\n"; });
    auto f1B = f1.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "F1 TaskB\n"; });
    f1A.name("f1A");
    f1B.name("f1B");

    //
    // f1_module_task ---> f2A ---> f2B ---> f2C
    //
    tf::Taskflow f2("F2");
    auto f2A = f2.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "- F2 TaskA\n"; });
    auto f2B = f2.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "- F2 TaskB\n"; });
    auto f2C = f2.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "- F2 TaskC\n"; });
    f2A.name("f2A");
    f2B.name("f2B");
    f2C.name("f2C");

    f2A.precede(f2B);
    f2B.precede(f2C);
    auto f1_module_task = f2.composed_of(f1).name("module_of_f1");
    f1_module_task.precede(f2A);

    //
    //                             | ---> f3B
    // f2_module_task ---> f3A --- |
    //                             | ---> f3C
    //
    tf::Taskflow f3("F3");
    auto f2_module_task = f3.composed_of(f2).name("module_of_f2");
    auto f3A = f3.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "- - F3 TaskA\n"; }).name("f3A");
    auto f3B = f3.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "- - F3 TaskB\n"; }).name("f3B");
    auto f3C = f3.emplace([&](){ std::lock_guard<std::mutex> lock(mtx); resGraph += "- - F3 TaskC\n"; }).name("f3C");

    f2_module_task.precede(f3A);
    f3A.precede(f3B, f3C);

    // f3.dump(std::cout);

    executor.run(f3).wait();

    // Because the task F1 A and B an d F3 B and C are independent four outcomes are possibles:
    const std::string expectedResult1 = "F1 TaskB\n""F1 TaskA\n""- F2 TaskA\n""- F2 TaskB\n"
                                        "- F2 TaskC\n""- - F3 TaskA\n""- - F3 TaskC\n""- - F3 TaskB\n";

    const std::string expectedResult2 = "F1 TaskB\n""F1 TaskA\n""- F2 TaskA\n""- F2 TaskB\n"
                                        "- F2 TaskC\n""- - F3 TaskA\n""- - F3 TaskB\n""- - F3 TaskC\n";

    const std::string expectedResult3 = "F1 TaskA\n""F1 TaskB\n""- F2 TaskA\n""- F2 TaskB\n"
                                        "- F2 TaskC\n""- - F3 TaskA\n""- - F3 TaskC\n""- - F3 TaskB\n";

    const std::string expectedResult4 = "F1 TaskA\n""F1 TaskB\n""- F2 TaskA\n""- F2 TaskB\n"
                                        "- F2 TaskC\n""- - F3 TaskA\n""- - F3 TaskB\n""- - F3 TaskC\n";

    auto expected = (resGraph == expectedResult1) or
                    (resGraph == expectedResult2) or
                    (resGraph == expectedResult3) or
                    (resGraph == expectedResult4);

    EXPECT_TRUE(expected);

    if(not expected)
        std::cout << resGraph << std::endl;
}

// ----------------------------------------------------------------------------------------
// ---------------------------        Test separator        -------------------------------
// ----------------------------------------------------------------------------------------
TEST(taskflow_test, pipeline)
{
    tf::Taskflow taskflow("pipeline");
    tf::Executor executor;

    const size_t num_lines = 4;

    // custom data storage
    std::array<int, num_lines> buffer;

    // the pipeline consists of three pipes (serial-parallel-serial)
    // and up to four concurrent scheduling tokens
    tf::Pipeline pl(num_lines,
        tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf) {
        // generate only 5 scheduling tokens
        if(pf.token() == 5) {
            pf.stop();
        }
        // save the result of this pipe into the buffer
        else {
            // printf("stage 1: input token = %zu\n", pf.token());
            buffer[pf.line()] = pf.token();
        }
        }},

        tf::Pipe{tf::PipeType::PARALLEL, [&buffer](tf::Pipeflow& pf) {
        // printf(
        //     "stage 2: input buffer[%zu] = %d\n", pf.line(), buffer[pf.line()]
        // );
        // propagate the previous result to this pipe and increment
        // it by one
        buffer[pf.line()] = buffer[pf.line()] + 1;
        }},

        tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf) {
        // printf(
        //     "stage 3: input buffer[%zu] = %d\n", pf.line(), buffer[pf.line()]
        // );
        // propagate the previous result to this pipe and increment
        // it by one
        buffer[pf.line()] = buffer[pf.line()] + 1;
        }}
    );

    // build the pipeline graph using composition
    tf::Task init = taskflow.emplace([](){})
                            .name("starting pipeline");
    tf::Task task = taskflow.composed_of(pl)
                            .name("pipeline");
    tf::Task stop = taskflow.emplace([](){})
                            .name("pipeline stopped");

    // create task dependency
    init.precede(task);
    task.precede(stop);

    // dump the pipeline graph structure (with composition)
    // taskflow.dump(std::cout);

    // run the pipeline
    executor.run(taskflow).wait();
}

// ----------------------------------------------------------------------------------------
// ---------------------------        Test separator        -------------------------------
// ----------------------------------------------------------------------------------------
TEST(taskflow_test, observer)
{
    tf::Executor executor;

    // Create a taskflow of eight tasks
    tf::Taskflow taskflow;

    taskflow.emplace([] () {}).name("A");
    taskflow.emplace([] () {}).name("B");
    taskflow.emplace([] () {}).name("C");
    taskflow.emplace([] () {}).name("D");
    taskflow.emplace([] () {}).name("E");
    taskflow.emplace([] () {}).name("F");
    taskflow.emplace([] () {}).name("G");
    taskflow.emplace([] () {}).name("H");

    // create a default observer
    std::shared_ptr<MyObserver> observer = executor.make_observer<MyObserver>("MyObserver");

    // run the taskflow
    executor.run(taskflow).get();

    // remove the observer (optional)
    executor.remove_observer(std::move(observer));
}