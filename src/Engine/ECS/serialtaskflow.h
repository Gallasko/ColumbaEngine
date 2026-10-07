#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "logger.h"

namespace pg
{
    /**
     * @brief The part of taskflow the ecs uses, run on the calling thread
     *
     * Used by builds that have no thread to give (PG_NO_THREADS: a web page that is not cross-origin isolated).
     * The tasks of a graph run one after the other, in the order their dependencies ask for and, between
     * tasks that do not depend on each other, in the order they were added.
     */
    namespace serial
    {
        struct TaskNode
        {
            std::function<void()> work;

            std::string name;

            /** The tasks that wait for this one */
            std::vector<TaskNode*> successors;

            /** The tasks this one waits for */
            std::vector<TaskNode*> dependencies;
        };

        class Taskflow;

        class Task
        {
        friend class Taskflow;
        public:
            Task() {}

            Task& name(const std::string& name)
            {
                if (node)
                    node->name = name;

                return *this;
            }

            /** This task runs after the other one */
            Task& succeed(const Task& other);

            /** This task runs before the other one */
            Task& precede(const Task& other);

        private:
            Task(TaskNode* node, Taskflow* graph) : node(node), graph(graph) {}

            TaskNode* node = nullptr;

            Taskflow* graph = nullptr;
        };

        class Taskflow
        {
        friend class Task;
        public:
            template <typename Func>
            Task emplace(Func&& work)
            {
                nodes.push_back(std::make_unique<TaskNode>());

                nodes.back()->work = std::forward<Func>(work);

                changed = true;

                return Task{nodes.back().get(), this};
            }

            void erase(const Task& task)
            {
                if (not task.node)
                    return;

                for (auto& node : nodes)
                {
                    unlink(node->successors, task.node);
                    unlink(node->dependencies, task.node);
                }

                // A run under way steps over what is gone
                for (auto& node : order)
                {
                    if (node == task.node)
                        node = nullptr;
                }

                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    if (nodes[i].get() == task.node)
                    {
                        nodes.erase(nodes.begin() + i);

                        break;
                    }
                }

                changed = true;
            }

            /** Runs every task once. A task added during the run waits for the next one */
            void runOnce()
            {
                if (changed)
                    sort();

                // By index: a task may add or erase tasks, never reorder the run under way
                for (size_t i = 0; i < order.size(); ++i)
                {
                    auto node = order[i];

                    if (not node or not node->work)
                        continue;

                    // An exception stops its task and not the others, as on a taskflow executor
                    try
                    {
                        node->work();
                    }
                    catch (const std::exception& e)
                    {
                        LOG_ERROR("Serial Taskflow", "Exception thrown by task '" << node->name << "': " << e.what());
                    }
                }
            }

            /** Graphviz DOT, in the shape taskflow gives its own dump */
            void dump(std::ostream& os) const
            {
                os << "digraph Taskflow {\n";
                os << "subgraph cluster_p" << this << " {\n";
                os << "label=\"Taskflow: p" << this << "\";\n";

                for (const auto& node : nodes)
                {
                    os << "p" << node.get() << "[label=\"" << node->name << "\" ];\n";

                    for (const auto& next : node->successors)
                        os << "p" << node.get() << " -> p" << next << ";\n";
                }

                os << "}\n";
                os << "}\n";
            }

        private:
            static void unlink(std::vector<TaskNode*>& list, TaskNode* node)
            {
                for (size_t i = 0; i < list.size();)
                {
                    if (list[i] == node)
                        list.erase(list.begin() + i);
                    else
                        ++i;
                }
            }

            void link(TaskNode* before, TaskNode* after)
            {
                if (not before or not after or before == after)
                    return;

                before->successors.push_back(after);
                after->dependencies.push_back(before);

                changed = true;
            }

            /** Every task after the ones it waits for; the oldest first among those that are free to run */
            void sort()
            {
                order.clear();
                order.reserve(nodes.size());

                std::vector<TaskNode*> placed;
                placed.reserve(nodes.size());

                auto isPlaced = [&placed](TaskNode* node) {
                    for (const auto& p : placed)
                    {
                        if (p == node)
                            return true;
                    }

                    return false;
                };

                bool progress = true;

                while (progress and placed.size() < nodes.size())
                {
                    progress = false;

                    for (const auto& node : nodes)
                    {
                        if (isPlaced(node.get()))
                            continue;

                        bool ready = true;

                        for (const auto& dependency : node->dependencies)
                            ready = ready and isPlaced(dependency);

                        if (not ready)
                            continue;

                        placed.push_back(node.get());
                        order.push_back(node.get());

                        progress = true;
                    }
                }

                // Tasks that wait for each other never become ready: they run last, as they were added
                if (placed.size() < nodes.size())
                {
                    LOG_ERROR("Serial Taskflow", "The task graph has a cycle, " << (nodes.size() - placed.size()) << " tasks run out of order");

                    for (const auto& node : nodes)
                    {
                        if (not isPlaced(node.get()))
                            order.push_back(node.get());
                    }
                }

                changed = false;
            }

            std::vector<std::unique_ptr<TaskNode>> nodes;

            std::vector<TaskNode*> order;

            bool changed = true;
        };

        inline Task& Task::succeed(const Task& other)
        {
            if (graph)
                graph->link(other.node, node);

            return *this;
        }

        inline Task& Task::precede(const Task& other)
        {
            if (graph)
                graph->link(node, other.node);

            return *this;
        }

        /** What run() gives back: the run is already over */
        struct Finished
        {
            void wait() const {}
        };

        class Executor
        {
        public:
            Executor(size_t) {}

            Finished run(Taskflow& graph)
            {
                graph.runOnce();

                return Finished{};
            }

            template <typename Predicate>
            Finished run_until(Taskflow& graph, Predicate&& stop)
            {
                do
                {
                    graph.runOnce();
                }
                while (not stop());

                return Finished{};
            }

            void wait_for_all() {}
        };
    }
}
