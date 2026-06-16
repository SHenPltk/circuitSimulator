#pragma once

#include <memory>
#include <string>
#include <vector>

namespace circuit {

struct BulbResult {
    std::string identifier;
    int state = 0;               // 0 = off, 1 = on
    int declarationOrder = -1;
};

class CircuitSimulator {
public:
    CircuitSimulator();
    ~CircuitSimulator();
    
    CircuitSimulator(const CircuitSimulator&) = delete;
    CircuitSimulator& operator=(const CircuitSimulator&) = delete;
    CircuitSimulator(CircuitSimulator&&) noexcept;
    CircuitSimulator& operator=(CircuitSimulator&&) noexcept;
    
    /// Executes a single command line.
    /// Returns true to keep the interpreter running.
    /// Returns false only when the 'exit' command is received.
    /// Errors are pushed into errorLines and do not cause exit.
    bool executeCommand(
        const std::string& commandLine,
        std::vector<std::string>& outputLines,
        std::vector<std::string>& errorLines);
    
    // Direct API (alternative to DSL)
    bool addComponent(const std::string& type,
                      const std::string& identifier);
    
    bool addComponents(const std::string& type,
                       const std::vector<std::string>& identifiers);
    
    bool addLink(const std::string& endpoint1,
                 const std::string& endpoint2);
    
    bool removeLink(const std::string& endpoint1,
                    const std::string& endpoint2);
    
    bool setSwitchState(const std::string& identifier,
                        bool closed);
    
    bool addSwitchGroup(
        const std::string& groupName,
        const std::vector<std::string>& switchIdentifiers);
    
    bool setSwitchGroupState(
        const std::string& groupName,
        const std::string& value);
    
    std::vector<BulbResult> simulate() const;
    
    std::string toString() const;
    
    void clearAll();
    void clearComponents();
    void clearLinks();
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace circuit