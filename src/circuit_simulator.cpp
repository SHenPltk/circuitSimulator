#include "circuit/circuit_simulator.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace circuit {

// Internal types to support the implementation
namespace {

enum class ComponentType { Power, Resistor, Switch, Bulb };

struct BulbState {
    std::string identifier;
    bool on = false;
    int declarationOrder = -1;
};

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string StripComment(const std::string& text) {
    const std::size_t pos = text.find('#');
    if (pos == std::string::npos) {
        return text;
    }
    return text.substr(0, pos);
}

std::string ToLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::vector<std::string> SplitWords(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::pair<std::string, std::string> SplitCommand(const std::string& line) {
    std::size_t pos = 0;
    while (pos < line.size() &&
           std::isspace(static_cast<unsigned char>(line[pos])) == 0) {
        ++pos;
    }
    return {line.substr(0, pos), Trim(line.substr(pos))};
}

std::vector<std::string> SplitCommaList(const std::string& text) {
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::size_t end =
            (comma == std::string::npos) ? text.size() : comma;
        const std::string item = Trim(text.substr(start, end - start));
        if (item.empty()) {
            throw std::runtime_error("Identifier list contains an empty item");
        }
        items.push_back(item);
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return items;
}

bool IsValidIdentifier(const std::string& identifier) {
    if (identifier.empty()) {
        return false;
    }

    const unsigned char first =
        static_cast<unsigned char>(identifier.front());
    if (!(std::isalpha(first) != 0 || identifier.front() == '_')) {
        return false;
    }

    for (char ch : identifier) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (!(std::isalnum(value) != 0 || ch == '_')) {
            return false;
        }
    }
    return true;
}

bool IsComponentType(const std::string& type_name) {
    const std::string value = ToLower(type_name);
    return value == "power" || value == "resistor" ||
           value == "protect_resistor" ||
           value == "protection_resistor" || value == "switch" ||
           value == "bulb" || value == "lamp";
}

ComponentType ParseComponentType(const std::string& type_name) {
    const std::string value = ToLower(type_name);
    if (value == "power") {
        return ComponentType::Power;
    }
    if (value == "resistor" || value == "protect_resistor" ||
        value == "protection_resistor") {
        return ComponentType::Resistor;
    }
    if (value == "switch") {
        return ComponentType::Switch;
    }
    if (value == "bulb" || value == "lamp") {
        return ComponentType::Bulb;
    }
    throw std::runtime_error("Unsupported component type: " + type_name);
}

std::string ComponentTypeName(ComponentType type) {
    switch (type) {
        case ComponentType::Power:
            return "power";
        case ComponentType::Resistor:
            return "resistor";
        case ComponentType::Switch:
            return "switch";
        case ComponentType::Bulb:
            return "bulb";
    }
    throw std::runtime_error("Unknown component type");
}

std::pair<std::string, std::string> ParseEndpointPair(
    const std::string& text,
    const std::string& command_name) {
    const std::size_t comma = text.find(',');
    if (comma == std::string::npos) {
        throw std::runtime_error(
            command_name + " must be: " + command_name +
            " endpoint1,endpoint2");
    }

    const std::string left = Trim(text.substr(0, comma));
    const std::string right = Trim(text.substr(comma + 1));
    if (left.empty() || right.empty()) {
        throw std::runtime_error(
            command_name + " must contain two endpoints");
    }
    return {left, right};
}

class DisjointSet {
public:
    explicit DisjointSet(int size) : parent_(size), rank_(size, 0) {
        for (int i = 0; i < size; ++i) {
            parent_[i] = i;
        }
    }

    int Find(int x) {
        if (parent_[x] != x) {
            parent_[x] = Find(parent_[x]);
        }
        return parent_[x];
    }

    void Unite(int a, int b) {
        a = Find(a);
        b = Find(b);
        if (a == b) {
            return;
        }
        if (rank_[a] < rank_[b]) {
            std::swap(a, b);
        }
        parent_[b] = a;
        if (rank_[a] == rank_[b]) {
            ++rank_[a];
        }
    }

private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

struct PositiveEdge {
    int u = -1;
    int v = -1;
    int component_index = -1;
};

int CountNodes(const std::vector<PositiveEdge>& graph_edges) {
    int node_count = 0;
    for (const auto& edge : graph_edges) {
        node_count = std::max(node_count, std::max(edge.u, edge.v) + 1);
    }
    return node_count;
}

unsigned long long ParseNumberLiteral(const std::string& text) {
    if (text.empty()) {
        throw std::runtime_error("Empty number value");
    }

    std::string value = text;
    int base = 10;
    if (value.size() >= 3 && value[0] == '0') {
        const unsigned char prefix =
            static_cast<unsigned char>(std::tolower(value[1]));
        if (prefix == 'x') {
            base = 16;
            value = value.substr(2);
        } else if (prefix == 'b') {
            base = 2;
            value = value.substr(2);
        }
    }

    if (value.empty()) {
        throw std::runtime_error("Empty number value after prefix: " + text);
    }

    unsigned long long result = 0;
    for (const char original_ch : value) {
        const unsigned char ch =
            static_cast<unsigned char>(std::tolower(original_ch));
        int digit = -1;
        if (std::isdigit(ch) != 0) {
            digit = ch - '0';
        } else if (base == 16 && ch >= 'a' && ch <= 'f') {
            digit = 10 + (ch - 'a');
        } else {
            throw std::runtime_error("Invalid number value: " + text);
        }
        if (digit < 0 || digit >= base) {
            throw std::runtime_error("Invalid digit for base " +
                                     std::to_string(base) + ": " + text);
        }
        result = result * static_cast<unsigned long long>(base) +
                 static_cast<unsigned long long>(digit);
    }
    return result;
}

}  // namespace

class CircuitSimulator::Impl {
public:
    struct Component {
        ComponentType type;
        std::string identifier;
        int pin1 = -1;
        int pin2 = -1;
        bool switch_closed = false;
        int declaration_order = -1;
    };

    struct LinkRecord {
        int left_pin = -1;
        int right_pin = -1;
        std::string left_text;
        std::string right_text;
    };

    struct ResolvedEndpoint {
        int pin = -1;
        std::string canonical_text;
    };

    struct BulbEdgeInfo {
        int graph_edge_index = -1;
        bool shorted = false;
    };

    bool ExecuteCommand(const std::string& raw_line,
                        std::vector<std::string>& output_lines,
                        std::vector<std::string>& error_lines) {
        try {
            const std::string line = Trim(StripComment(raw_line));
            if (line.empty()) {
                return true;
            }

            const auto [command_word, rest] = SplitCommand(line);
            const std::string command = ToLower(command_word);

            if (Impl::IsComponentType(command)) {
                if (rest.empty()) {
                    error_lines.push_back("Component definition must be: [type] [identifier]");
                    return true;
                }
                DefineComponents(ParseComponentType(command), SplitCommaList(rest));
                return true;
            }

            if (command == "link") {
                const auto endpoints = ParseEndpointPair(rest, "link");
                AddLink(endpoints.first, endpoints.second);
                return true;
            }

            if (command == "del") {
                const auto endpoints = ParseEndpointPair(rest, "del");
                if (!DeleteLink(endpoints.first, endpoints.second)) {
                    error_lines.push_back("Link does not exist");
                    return true;
                }
                return true;
            }

            if (command == "set") {
                const auto tokens = SplitWords(rest);
                if (tokens.size() != 2) {
                    error_lines.push_back(
                        "set must be: set [switch] 0/1 or set [groupName] [integer]");
                    return true;
                }

                if (switch_groups_.count(tokens[0]) != 0U) {
                    const int value =
                        static_cast<int>(ParseNumberLiteral(tokens[1]));
                    SetSwitchGroupState(tokens[0], value);
                    return true;
                }

                if (tokens[1] != "0" && tokens[1] != "1") {
                    error_lines.push_back("Switch state must be 0 or 1");
                    return true;
                }
                SetSwitch(tokens[0], tokens[1] == "1");
                return true;
            }

            if (command == "run") {
                if (!rest.empty()) {
                    error_lines.push_back("run does not accept arguments");
                    return true;
                }
                auto bulbs = Run();
                for (const auto& bulb : bulbs) {
                    output_lines.push_back(bulb.identifier + " " + (bulb.on ? "1" : "0"));
                }
                return true;
            }

            if (command == "print") {
                if (!rest.empty()) {
                    error_lines.push_back("print does not accept arguments");
                    return true;
                }
                output_lines.push_back(PrintState());
                return true;
            }

            if (command == "clear") {
                const std::string target = ToLower(rest);
                if (target.empty() || target == "all") {
                    Clear();
                    return true;
                }
                if (target == "links") {
                    ClearLinks();
                    return true;
                }
                if (target == "components") {
                    ClearComponents();
                    return true;
                }
                error_lines.push_back(
                    "clear only supports: clear, clear all, clear links, clear components");
                return true;
            }

            if (command == "group") {
                const auto tokens = SplitWords(rest);
                if (tokens.size() != 2) {
                    error_lines.push_back(
                        "group must be: group [groupName] s1,s2,...");
                    return true;
                }
                AddSwitchGroup(tokens[0], SplitCommaList(tokens[1]));
                return true;
            }

            if (command == "exit") {
                if (!rest.empty()) {
                    error_lines.push_back("exit does not accept arguments");
                    return true;
                }
                return false;  // Return false only to indicate exit
            }

            error_lines.push_back("Unknown command: " + command_word);
            return true;
        } catch (const std::exception& e) {
            error_lines.push_back(e.what());
            return true;
        } catch (...) {
            error_lines.push_back("Unknown error");
            return true;
        }
    }

    void DefineComponents(
        ComponentType type,
        const std::vector<std::string>& identifiers) {
        for (const std::string& identifier : identifiers) {
            if (!IsValidIdentifier(identifier)) {
                throw std::runtime_error(
                    "Invalid identifier: " + identifier);
            }
            if (components_by_id_.count(identifier) != 0U) {
                throw std::runtime_error(
                    "Duplicate identifier: " + identifier);
            }
            if (switch_groups_.count(identifier) != 0U) {
                throw std::runtime_error(
                    "Identifier conflicts with switch group: " + identifier);
            }

            Component component;
            component.type = type;
            component.identifier = identifier;
            component.pin1 = next_pin_++;
            component.pin2 = next_pin_++;
            component.declaration_order =
                static_cast<int>(components_.size());

            if (type == ComponentType::Power) {
                if (power_index_ != -1) {
                    throw std::runtime_error(
                        "Only one power component is supported");
                }
                power_index_ = static_cast<int>(components_.size());
            }

            if (type == ComponentType::Bulb) {
                bulb_indices_.push_back(static_cast<int>(components_.size()));
            }

            components_by_id_[identifier] =
                static_cast<int>(components_.size());
            components_.push_back(component);
        }
    }

    void AddLink(
        const std::string& left_endpoint,
        const std::string& right_endpoint) {
        const ResolvedEndpoint left = ResolveEndpoint(left_endpoint);
        const ResolvedEndpoint right = ResolveEndpoint(right_endpoint);
        if (left.pin == right.pin) {
            throw std::runtime_error("A link cannot connect the same pin");
        }
        if (FindLink(left.pin, right.pin) != links_.end()) {
            throw std::runtime_error("Duplicate link");
        }

        links_.push_back(
            {left.pin, right.pin, left.canonical_text, right.canonical_text});
    }

    static bool IsComponentType(const std::string& type_name) {
        static auto ToLower = [](std::string value) {
            for (char& ch : value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        };
        
        const std::string value = ToLower(type_name);
        return value == "power" || value == "resistor" ||
               value == "protect_resistor" ||
               value == "protection_resistor" || value == "switch" ||
               value == "bulb" || value == "lamp";
    }

    static ComponentType ParseComponentType(const std::string& type_name) {
        static auto ToLower = [](std::string value) {
            for (char& ch : value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        };
        
        const std::string value = ToLower(type_name);
        if (value == "power") {
            return ComponentType::Power;
        }
        if (value == "resistor" || value == "protect_resistor" ||
            value == "protection_resistor") {
            return ComponentType::Resistor;
        }
        if (value == "switch") {
            return ComponentType::Switch;
        }
        if (value == "bulb" || value == "lamp") {
            return ComponentType::Bulb;
        }
        throw std::runtime_error("Unsupported component type: " + type_name);
    }

    bool DeleteLink(
        const std::string& left_endpoint,
        const std::string& right_endpoint) {
        const ResolvedEndpoint left = ResolveEndpoint(left_endpoint);
        const ResolvedEndpoint right = ResolveEndpoint(right_endpoint);
        auto it = FindLink(left.pin, right.pin);
        if (it == links_.end()) {
            return false;
        }
        links_.erase(it);
        return true;
    }

    void SetSwitch(const std::string& identifier, bool closed) {
        if (!IsValidIdentifier(identifier)) {
            throw std::runtime_error("Invalid identifier: " + identifier);
        }

        const auto it = components_by_id_.find(identifier);
        if (it == components_by_id_.end()) {
            throw std::runtime_error("Unknown component: " + identifier);
        }

        Component& component = components_[it->second];
        if (component.type != ComponentType::Switch) {
            throw std::runtime_error(identifier + " is not a switch");
        }
        component.switch_closed = closed;
    }

    void AddSwitchGroup(
        const std::string& group_name,
        const std::vector<std::string>& identifiers) {
        if (!IsValidIdentifier(group_name)) {
            throw std::runtime_error("Invalid group name: " + group_name);
        }
        if (components_by_id_.count(group_name) != 0U) {
            throw std::runtime_error(
                "Group name conflicts with component: " + group_name);
        }
        if (switch_groups_.count(group_name) != 0U) {
            throw std::runtime_error("Duplicate group name: " + group_name);
        }
        if (identifiers.empty()) {
            throw std::runtime_error(
                "Switch group must contain at least one switch");
        }
        if (identifiers.size() > 64U) {
            throw std::runtime_error(
                "Switch groups cannot contain more than 64 switches");
        }

        std::vector<std::string> validated;
        validated.reserve(identifiers.size());
        for (const auto& identifier : identifiers) {
            if (!IsValidIdentifier(identifier)) {
                throw std::runtime_error(
                    "Invalid switch identifier: " + identifier);
            }
            const auto it = components_by_id_.find(identifier);
            if (it == components_by_id_.end()) {
                throw std::runtime_error(
                    "Unknown component in group: " + identifier);
            }
            const Component& component = components_[it->second];
            if (component.type != ComponentType::Switch) {
                throw std::runtime_error(identifier + " is not a switch");
            }
            validated.push_back(identifier);
        }
        switch_groups_[group_name] = std::move(validated);
    }

    void SetSwitchGroupState(
        const std::string& group_name,
        const int value) {
        const auto it = switch_groups_.find(group_name);
        if (it == switch_groups_.end()) {
            throw std::runtime_error("Unknown switch group: " + group_name);
        }

        const unsigned long long bits = static_cast<unsigned long long>(value);
        const auto& switches = it->second;
        for (std::size_t i = 0; i < switches.size(); ++i) {
            const bool closed = (bits & (1ULL << i)) != 0ULL;
            SetSwitch(switches[i], closed);
        }
    }

    std::vector<BulbState> Run() const {
        if (power_index_ == -1) {
            throw std::runtime_error("No power component defined");
        }

        DisjointSet dsu(next_pin_);
        for (const auto& link : links_) {
            dsu.Unite(link.left_pin, link.right_pin);
        }
        for (const auto& component : components_) {
            if (component.type == ComponentType::Switch &&
                component.switch_closed) {
                dsu.Unite(component.pin1, component.pin2);
            }
        }

        std::unordered_map<int, int> node_by_root;
        std::vector<int> roots_by_node;
        auto get_node_index = [&](int pin) -> int {
            const int root = dsu.Find(pin);
            const auto it = node_by_root.find(root);
            if (it != node_by_root.end()) {
                return it->second;
            }
            const int node = static_cast<int>(roots_by_node.size());
            node_by_root[root] = node;
            roots_by_node.push_back(root);
            return node;
        };

        const Component& power = components_[power_index_];
        const int source_positive = get_node_index(power.pin1);
        const int source_negative = get_node_index(power.pin2);

        std::vector<PositiveEdge> graph_edges;
        std::vector<BulbEdgeInfo> bulb_infos(
            components_.size(), BulbEdgeInfo{});

        for (int i = 0; i < static_cast<int>(components_.size()); ++i) {
            const Component& component = components_[i];
            if (component.type != ComponentType::Resistor &&
                component.type != ComponentType::Bulb) {
                continue;
            }

            const int u = get_node_index(component.pin1);
            const int v = get_node_index(component.pin2);

            if (component.type == ComponentType::Bulb && u == v) {
                bulb_infos[i].shorted = true;
                continue;
            }
            if (u == v) {
                continue;
            }

            const int edge_index = static_cast<int>(graph_edges.size());
            graph_edges.push_back({u, v, i});
            if (component.type == ComponentType::Bulb) {
                bulb_infos[i].graph_edge_index = edge_index;
            }
        }

        std::vector<BulbState> results;
        results.reserve(bulb_indices_.size());

        if (source_positive == source_negative || graph_edges.empty()) {
            for (int bulb_index : bulb_indices_) {
                results.push_back(
                    {components_[bulb_index].identifier, false});
            }
            return results;
        }

        const std::vector<bool> blocks_on_path = FindBlocksOnSourcePath(
            source_positive, source_negative, graph_edges);

        for (int bulb_index : bulb_indices_) {
            const Component& bulb = components_[bulb_index];
            const BulbEdgeInfo& info = bulb_infos[bulb_index];

            bool state = false;
            if (!info.shorted && info.graph_edge_index != -1) {
                const int block_id = edge_to_block_[info.graph_edge_index];
                if (block_id != -1 &&
                    block_id < static_cast<int>(blocks_on_path.size()) &&
                    blocks_on_path[block_id]) {
                    state = true;
                }
            }
            results.push_back({bulb.identifier, state});
        }

        return results;
    }

    std::string PrintState() const {
        std::ostringstream output;

        if (components_.empty() && links_.empty()) {
            output << "# empty circuit\n";
            return output.str();
        }

        for (const auto& component : components_) {
            output << ComponentTypeName(component.type) << ' '
                   << component.identifier << '\n';
        }
        for (const auto& component : components_) {
            if (component.type == ComponentType::Switch) {
                output << "set " << component.identifier << ' '
                       << (component.switch_closed ? '1' : '0') << '\n';
            }
        }
        for (const auto& entry : switch_groups_) {
            output << "group " << entry.first;
            for (std::size_t i = 0; i < entry.second.size(); ++i) {
                output << (i == 0U ? " " : ",") << entry.second[i];
            }
            output << '\n';
        }
        for (const auto& link : links_) {
            output << "link " << link.left_text << ',' << link.right_text
                   << '\n';
        }
        return output.str();
    }

    void Clear() {
        components_.clear();
        components_by_id_.clear();
        links_.clear();
        bulb_indices_.clear();
        edge_to_block_.clear();
        switch_groups_.clear();
        next_pin_ = 0;
        power_index_ = -1;
    }

    void ClearLinks() {
        links_.clear();
        edge_to_block_.clear();
    }

    void ClearComponents() {
        Clear();
    }

private:
    ResolvedEndpoint ResolveEndpoint(const std::string& endpoint) const {
        const std::size_t dot = endpoint.find('.');
        if (dot == std::string::npos) {
            throw std::runtime_error(
                "Endpoint must be identifier.port, got: " + endpoint);
        }

        const std::string identifier = endpoint.substr(0, dot);
        const std::string port = ToLower(endpoint.substr(dot + 1));
        if (!IsValidIdentifier(identifier)) {
            throw std::runtime_error("Invalid identifier: " + identifier);
        }

        const auto it = components_by_id_.find(identifier);
        if (it == components_by_id_.end()) {
            throw std::runtime_error("Unknown component: " + identifier);
        }

        const Component& component = components_[it->second];
        if (IsFirstPinName(component.type, port)) {
            return {component.pin1, CanonicalPortName(component, true)};
        }
        if (IsSecondPinName(component.type, port)) {
            return {component.pin2, CanonicalPortName(component, false)};
        }

        throw std::runtime_error(
            "Invalid port for " + identifier + ": " + port);
    }

    static bool IsFirstPinName(ComponentType type, const std::string& name) {
        if (type == ComponentType::Power) {
            return name == "+" || name == "1" || name == "p" ||
                   name == "pos" || name == "positive";
        }
        return name == "1" || name == "a" || name == "l" ||
               name == "left";
    }

    static bool IsSecondPinName(ComponentType type, const std::string& name) {
        if (type == ComponentType::Power) {
            return name == "-" || name == "2" || name == "n" ||
                   name == "neg" || name == "negative";
        }
        return name == "2" || name == "b" || name == "r" ||
               name == "right";
    }

    static std::string CanonicalPortName(
        const Component& component,
        bool first_pin) {
        if (component.type == ComponentType::Power) {
            return component.identifier + (first_pin ? ".+" : ".-");
        }
        return component.identifier + (first_pin ? ".1" : ".2");
    }

    std::vector<LinkRecord>::iterator FindLink(int left_pin, int right_pin) {
        return std::find_if(
            links_.begin(), links_.end(),
            [&](const LinkRecord& link) {
                return (link.left_pin == left_pin &&
                        link.right_pin == right_pin) ||
                       (link.left_pin == right_pin &&
                        link.right_pin == left_pin);
            });
    }

    std::vector<LinkRecord>::const_iterator FindLink(
        int left_pin,
        int right_pin) const {
        return std::find_if(
            links_.begin(), links_.end(),
            [&](const LinkRecord& link) {
                return (link.left_pin == left_pin &&
                        link.right_pin == right_pin) ||
                       (link.left_pin == right_pin &&
                        link.right_pin == left_pin);
            });
    }

    std::vector<bool> FindBlocksOnSourcePath(
        int source_positive,
        int source_negative,
        const std::vector<PositiveEdge>& graph_edges) const {
        const int node_count = CountNodes(graph_edges);
        std::vector<std::vector<std::pair<int, int>>> adjacency(node_count);
        for (int i = 0; i < static_cast<int>(graph_edges.size()); ++i) {
            const auto& edge = graph_edges[i];
            adjacency[edge.u].push_back({edge.v, i});
            adjacency[edge.v].push_back({edge.u, i});
        }

        std::vector<int> discovery(node_count, 0);
        std::vector<int> low(node_count, 0);
        std::vector<int> edge_stack;
        std::vector<std::vector<int>> blocks;
        edge_to_block_.assign(graph_edges.size(), -1);
        int current_time = 0;

        const auto build_block = [&](int stop_edge) {
            std::vector<int> block_edges;
            while (!edge_stack.empty()) {
                const int edge_index = edge_stack.back();
                edge_stack.pop_back();
                block_edges.push_back(edge_index);
                if (edge_index == stop_edge) {
                    break;
                }
            }
            const int block_id = static_cast<int>(blocks.size());
            for (int edge_index : block_edges) {
                edge_to_block_[edge_index] = block_id;
            }
            blocks.push_back(std::move(block_edges));
        };

        std::function<void(int, int)> dfs = [&](int u, int parent_edge) {
            discovery[u] = low[u] = ++current_time;
            for (const auto& next : adjacency[u]) {
                const int v = next.first;
                const int edge_index = next.second;
                if (edge_index == parent_edge) {
                    continue;
                }
                if (discovery[v] == 0) {
                    edge_stack.push_back(edge_index);
                    dfs(v, edge_index);
                    low[u] = std::min(low[u], low[v]);
                    if (low[v] >= discovery[u]) {
                        build_block(edge_index);
                    }
                } else if (discovery[v] < discovery[u]) {
                    edge_stack.push_back(edge_index);
                    low[u] = std::min(low[u], discovery[v]);
                }
            }
        };

        for (int node = 0; node < node_count; ++node) {
            if (discovery[node] != 0) {
                continue;
            }
            dfs(node, -1);
            if (!edge_stack.empty()) {
                build_block(edge_stack.back());
            }
        }

        std::vector<std::vector<int>> blocks_by_vertex(node_count);
        for (int block_id = 0; block_id < static_cast<int>(blocks.size());
             ++block_id) {
            std::unordered_set<int> unique_vertices;
            for (int edge_index : blocks[block_id]) {
                unique_vertices.insert(graph_edges[edge_index].u);
                unique_vertices.insert(graph_edges[edge_index].v);
            }
            for (int vertex : unique_vertices) {
                blocks_by_vertex[vertex].push_back(block_id);
            }
        }

        std::vector<int> articulation_node(node_count, -1);
        int articulation_count = 0;
        for (int vertex = 0; vertex < node_count; ++vertex) {
            if (blocks_by_vertex[vertex].size() > 1U) {
                articulation_node[vertex] =
                    static_cast<int>(blocks.size()) + articulation_count++;
            }
        }

        std::vector<std::vector<int>> tree(
            static_cast<int>(blocks.size()) + articulation_count);
        for (int vertex = 0; vertex < node_count; ++vertex) {
            if (articulation_node[vertex] == -1) {
                continue;
            }
            const int articulation_id = articulation_node[vertex];
            for (int block_id : blocks_by_vertex[vertex]) {
                tree[articulation_id].push_back(block_id);
                tree[block_id].push_back(articulation_id);
            }
        }

        const auto find_tree_node = [&](int vertex) -> int {
            if (vertex < 0 || vertex >= node_count) {
                return -1;
            }
            if (articulation_node[vertex] != -1) {
                return articulation_node[vertex];
            }
            if (blocks_by_vertex[vertex].empty()) {
                return -1;
            }
            return blocks_by_vertex[vertex][0];
        };

        const int start_node = find_tree_node(source_positive);
        const int target_node = find_tree_node(source_negative);

        std::vector<bool> block_on_path(blocks.size(), false);
        if (start_node == -1 || target_node == -1) {
            return block_on_path;
        }

        std::vector<int> parent(tree.size(), -1);
        std::vector<int> queue;
        queue.push_back(start_node);
        for (int head = 0; head < static_cast<int>(queue.size()); ++head) {
            const int current = queue[head];
            if (current == target_node) {
                break;
            }
            for (int next : tree[current]) {
                if (next == start_node || parent[next] != -1) {
                    continue;
                }
                parent[next] = current;
                queue.push_back(next);
            }
        }

        if (start_node != target_node && parent[target_node] == -1) {
            return block_on_path;
        }

        int current = target_node;
        while (current != -1) {
            if (current < static_cast<int>(blocks.size())) {
                block_on_path[current] = true;
            }
            if (current == start_node) {
                break;
            }
            current = parent[current];
        }
        return block_on_path;
    }

    std::vector<Component> components_;
    std::unordered_map<std::string, int> components_by_id_;
    std::vector<LinkRecord> links_;
    std::vector<int> bulb_indices_;
    std::unordered_map<std::string, std::vector<std::string>> switch_groups_;
    int next_pin_ = 0;
    int power_index_ = -1;
    mutable std::vector<int> edge_to_block_;
};

CircuitSimulator::CircuitSimulator() : impl_(std::make_unique<Impl>()) {}

CircuitSimulator::~CircuitSimulator() = default;

CircuitSimulator::CircuitSimulator(CircuitSimulator&&) noexcept = default;

CircuitSimulator& CircuitSimulator::operator=(CircuitSimulator&&) noexcept = default;

bool CircuitSimulator::executeCommand(
    const std::string& commandLine,
    std::vector<std::string>& outputLines,
    std::vector<std::string>& errorLines) {
    return impl_->ExecuteCommand(commandLine, outputLines, errorLines);
}

// Direct API implementation
bool CircuitSimulator::addComponent(const std::string& type,
                                     const std::string& identifier) {
    try {
        std::vector<std::string> identifiers = {identifier};
        impl_->DefineComponents(Impl::ParseComponentType(type), identifiers);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool CircuitSimulator::addComponents(const std::string& type,
                                      const std::vector<std::string>& identifiers) {
    try {
        impl_->DefineComponents(Impl::ParseComponentType(type), identifiers);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool CircuitSimulator::addLink(const std::string& endpoint1,
                                const std::string& endpoint2) {
    try {
        impl_->AddLink(endpoint1, endpoint2);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool CircuitSimulator::removeLink(const std::string& endpoint1,
                                   const std::string& endpoint2) {
    try {
        return impl_->DeleteLink(endpoint1, endpoint2);
    } catch (const std::exception& e) {
        return false;
    }
}

bool CircuitSimulator::setSwitchState(const std::string& identifier,
                                       bool closed) {
    try {
        impl_->SetSwitch(identifier, closed);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool CircuitSimulator::addSwitchGroup(
    const std::string& groupName,
    const std::vector<std::string>& switchIdentifiers) {
    try {
        impl_->AddSwitchGroup(groupName, switchIdentifiers);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool CircuitSimulator::setSwitchGroupState(
    const std::string& groupName,
    const int value) {
    try {
        impl_->SetSwitchGroupState(groupName, value);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<BulbResult> CircuitSimulator::simulate() const {
    try {
        auto bulbStates = impl_->Run();
        std::vector<BulbResult> results;
        results.reserve(bulbStates.size());
        
        for (const auto& state : bulbStates) {
            BulbResult result;
            result.identifier = state.identifier;
            result.state = state.on ? 1 : 0;
            result.declarationOrder = state.declarationOrder;
            results.push_back(result);
        }
        
        return results;
    } catch (...) {
        return {};
    }
}

std::string CircuitSimulator::toString() const {
    try {
        return impl_->PrintState();
    } catch (...) {
        return "";
    }
}

void CircuitSimulator::clearAll() {
    impl_->Clear();
}

void CircuitSimulator::clearComponents() {
    impl_->ClearComponents();
}

void CircuitSimulator::clearLinks() {
    impl_->ClearLinks();
}

}  // namespace circuit
