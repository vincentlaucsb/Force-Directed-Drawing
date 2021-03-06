#include "force_directed.h"
#include <chrono>

namespace force_directed {
    VertexPos random_layout(TUNGraph& graph) {
        VertexPos pos;

        // Iterate over vertices, assigning random coordinates
        typedef std::chrono::high_resolution_clock myclock;
        myclock::time_point beginning = myclock::now();

        // obtain a seed from the timer
        myclock::duration d = myclock::now() - beginning;
        unsigned seed = d.count();

        std::mt19937 generator(seed);
        std::uniform_real_distribution<double> distribution(0.0, 500.0);

        for (auto u = graph.BegNI(); u != graph.EndNI(); u++)
            pos[u.GetId()] = std::make_pair(distribution(generator), distribution(generator));

        return pos;
    }

    std::vector<SVG::SVG> eades84(TUNGraph& graph) {
        VertexPos pos = random_layout(graph);
        return eades84(graph, pos);
    }

    std::vector<SVG::SVG> eades84_2(ForceDirectedParams& params, TUNGraph& graph) {
        VertexPos pos = random_layout(graph);
        return eades84_2(params, graph, pos);
    }

    AdjacencyList adjacency_list(TUNGraph& graph) {
        // Compute adjacency list
        AdjacencyList adj;
        for (auto e = graph.BegEI(); e != graph.EndEI(); e++) {
            adj[e.GetSrcNId()].insert(e.GetDstNId());
            adj[e.GetDstNId()].insert(e.GetSrcNId());
        }

        return adj;
    }

    std::vector<SVG::SVG> eades84(TUNGraph& graph, VertexPos& pos) {
        /** An attempt to implement Eades' algorithm as described in his 1984 paper */
        AdjacencyList adj = adjacency_list(graph), not_adj;
        std::vector<SVG::SVG> ret;

        // Compute non-adjacency list
        for (auto& pair : adj) {
            auto& u = pair.first;
            auto& u_adj = pair.second;

            for (auto v = graph.BegNI(); v != graph.EndNI(); v++) {
                int v_id = v.GetId();
                if ((u_adj.find(v_id) == u_adj.end()) &&
                    u != v_id) not_adj[u].insert(v_id);
            }
        }

        const double c1 = 2.0, c2 = 1.0, c3 = 1.0, c4 = 0.1;
        const int m = 100;

        // Initial positions
        ret.push_back(draw_graph(graph, pos));

        for (int i = 0; i < m; i++) {
            // Calculate force on each vertex
            for (auto u = graph.BegNI(); u != graph.EndNI(); u++) {
                double force = 0;
                int u_id = u.GetId();

                // Iterate over adjacent vertices
                for (auto& v : adj[u_id]) {
                    force += c1 * log10(
                        sqrt(pow(pos[u_id].first - pos[v].first, 2)
                            + pow(pos[u_id].second - pos[v].second, 2))
                        / c2);
                }

                // Iterate over non-adjacent vertices
                for (auto& v : not_adj[u_id]) {
                    force += c3 / sqrt(sqrt(pow(pos[u_id].first - pos[v].first, 2)
                        + pow(pos[u_id].second - pos[v].second, 2)));
                }

                // Move vertex
                pos[u_id].first += (c4 * force);
                pos[u_id].second += (c4 * force);
            }

            ret.push_back(draw_graph(graph, pos));
        }

        return ret;
    }

    namespace eades84_helper {
        double distance_between(VertexPos& pos, int node1, int node2) {
            auto &pos1 = pos[node1], &pos2 = pos[node2];
            return sqrt(
                pow(pos1.first - pos2.first, 2) +
                pow(pos1.second - pos2.second, 2)
            );
        }

        Point calculate_force(ForceDirectedParams& params, TUNGraph& graph, int node,
            AdjacencyList& adjacent, VertexPos& pos) {
            // Calculate the force on one node
            double sum_x = 0, sum_y = 0;
            double& luv = params.luv, kuv1 = params.kuv1, kuv2 = params.kuv2;

            // Iterate over adjacent vertices
            for (auto adj : adjacent[node]) {
                double length = distance_between(pos, adj, node);
                sum_x += kuv1 * (length - luv) * (pos[node].first - pos[adj].first) / length;
                sum_y += kuv1 * (length - luv) * (pos[node].second - pos[adj].second) / length;
            }

            // Iterate over vertices X vertices
            for (auto vertex = graph.BegNI(); vertex < graph.EndNI(); vertex++) {
                int v_id = vertex.GetId();
                double dist = distance_between(pos, v_id, node);

                if (node != vertex.GetId()) {
                    sum_x += (kuv2 / pow(dist, 2)) * (pos[node].first - pos[v_id].first) / dist;
                    sum_y += (kuv2 / pow(dist, 2)) * (pos[node].second - pos[v_id].second) / dist;
                }
            }

            return std::make_pair(sum_x, sum_y);
        }
    }

    std::vector<SVG::SVG> eades84_2(ForceDirectedParams& params, TUNGraph& graph, VertexPos& pos) {
        /** Use Eades' spring layout algorithm, creating a frame between each iteration */
        std::vector<SVG::SVG> ret;
        AdjacencyList adjacent = adjacency_list(graph); // Optimization
        std::map<TUNGraph::TNodeI, Point> forces;
        ret.push_back(draw_graph(graph, pos)); // Record initial positions

        bool move = true;
        const int MAX_ITERATIONS = 1000;
        for (int i = 0; move && i < MAX_ITERATIONS; i++) {
            for (auto node = graph.BegNI(); node < graph.EndNI(); node++) {
                auto force = eades84_helper::calculate_force(params, graph, node.GetId(), adjacent, pos);
                forces[node] = force;
            }

            // Keep moving as long as forces not zero
            move = false;
            for (auto force : forces) {
                if (!APPROX_EQUALS(sqrt(pow(force.second.first, 2) - pow(force.second.second, 2)), 0, 5))
                    move = true;
            }

            // Move nodes...
            for (auto force : forces) {
                // ... in the direction of the force by a distance proportional to the magnitude of the force
                auto &force_x = force.second.first, &force_y = force.second.second;
                auto node = force.first.GetId();
                double pct = 0.1;

                if (isnan(force_x)) throw std::runtime_error("Failed to converge");
                pos[node].first -= pct * force_x;
                pos[node].second -= pct * force_y;
            }

            // Add frame
            ret.push_back(draw_graph(graph, pos));
        }

        return ret;
    }

    SVG::SVG draw_graph(TUNGraph& graph, VertexPos& pos, const double width) {
        SVG::SVG root;
        auto edges = root.add_child<SVG::Group>(), vertices = root.add_child<SVG::Group>();
        edges->set_attr("stroke", "black").set_attr("stroke-width", "1px");

        // Map IDs to nodes
        std::map<int, SVG::Circle*> nodes;
        const double circle_radius = std::max(5.0, width / 50);


        // Draw vertices
        for (auto node = graph.BegNI(); node < graph.EndNI(); node++) {
            auto coord = pos[node.GetId()];
            nodes[node.GetId()] = vertices->add_child<SVG::Circle>(coord, circle_radius);
        };

        for (auto edge = graph.BegEI(); edge < graph.EndEI(); edge++) {
            edges->add_child<SVG::Line>(
                nodes[edge.GetSrcNId()]->x(),
                nodes[edge.GetDstNId()]->x(),
                nodes[edge.GetSrcNId()]->y(),
                nodes[edge.GetDstNId()]->y()
                );
        }

        return root;
    }

    std::vector<SVG::SVG> barycenter_layout(TUNGraph& graph, const size_t fixed_vertices, const double width) {
        VertexPos pos;
        std::vector<SVG::SVG> ret;
        std::set<TUNGraph::TNodeI> fixed, free;
        std::vector<Point> polygon = SVG::util::polar_points((int)fixed_vertices, 0, 0, width/2);

        auto node = graph.BegNI();
        auto point = polygon.begin();
        for (; (node < graph.EndNI()) && (fixed.size() < fixed_vertices); node++) {
            fixed.insert(node);

            // Place along polygon
            pos[node.GetId()] = std::make_pair(point->first, point->second);
            point++;
        }

        for (; node < graph.EndNI(); node++) {
            free.insert(node);
            pos[node.GetId()] = std::make_pair(0, 0); // Place free vertices at origin
        }

        // Draw initial positions
        ret.push_back(draw_graph(graph, pos));

        // Optimization: Keep a list of adjacent vertices
        std::map<int, VertexSet> adjacent = adjacency_list(graph);

        bool converge;
        do {
            converge = true;
            for (auto node : free) {
                int node_id = node.GetId();
                auto current_xy = pos[node_id];
                double sum_x = 0, sum_y = 0, new_x, new_y;

                // Sum up adjacent vertices
                for (auto u : adjacent[node_id]) {
                    auto u_xy = pos[u];
                    sum_x += u_xy.first;
                    sum_y += u_xy.second;
                }

                pos[node_id].first = (1 / (double)node.GetDeg()) * sum_x;
                pos[node_id].second = (1 / (double)node.GetDeg()) * sum_y;

                // Convergence test
                if (!(APPROX_EQUALS(new_x, current_xy.first, 0.01) && APPROX_EQUALS(new_y, current_xy.second, 0.01)))
                    converge = false;
            }

            // Algorithm trace
            ret.push_back(draw_graph(graph, pos));
        } while (!converge);

        return ret;
    }

    BarycenterLayout barycenter_layout_la(
        TUNGraph& graph, const size_t fixed_vertices, const double width) {
        /** Solve the barycenter layout problem using linear algebra */
        std::vector<TUNGraph::TNodeI> fixed, free;
        std::vector<Point> polygon = SVG::util::polar_points((int)fixed_vertices, 0, 0, width / 2);
        VertexPos pos;

        auto node = graph.BegNI();
        auto point = polygon.begin();
        for (; (node < graph.EndNI()) && (fixed.size() < fixed_vertices); node++) {
            fixed.push_back(node);
            pos[node.GetId()] = std::make_pair(point->first, point->second); // Place along polygon
            point++;
        }

        for (; node < graph.EndNI(); node++) free.push_back(node);

        MatrixXd points(free.size(), free.size());
        VectorXd x(free.size()), y(free.size());;

        // Populate the matrix
        for (int i = 0; i < (int)free.size(); i++) {
            for (int j = 0; j < (int)free.size(); j++) {
                if (i == j) points(i, j) = (int)free[i].GetDeg();
                else if (free[i].IsNbrNId(free[j].GetId())) points(i, j) = -1;
                else points(i, j) = 0;
            }
        }

        // Populate vectors of fixed positions
        for (size_t i = 0; i < free.size(); i++) {
            // Sum up fixed vertices adjacent to our free boi
            double sum_x = 0, sum_y = 0;
            for (auto& f : fixed) {
                int f_id = f.GetId();
                if (free[i].IsNbrNId(f_id)) {
                    sum_x += pos[f_id].first;
                    sum_y += pos[f_id].second;
                }
            }

            x(i) = sum_x;
            y(i) = sum_y;
        }

        // std::cout << "Here is the matrix A:\n" << points << std::endl;
        // std::cout << "Here is the vector b:\n" << x << std::endl;
        VectorXd sol_x = points.fullPivHouseholderQr().solve(x),
            sol_y = points.fullPivHouseholderQr().solve(y);
        // std::cout << "The solution is:\n" << sol_x << std::endl;

        // Set graph positions
        for (size_t i = 0; i < sol_x.size(); i++)
            pos[free[i].GetId()] = std::make_pair(sol_x(i), sol_y(i));

        return { draw_graph(graph, pos), points, x, y, sol_x, sol_y };
    }
}