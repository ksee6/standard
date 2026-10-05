/**
 * @file Yaml.hpp
 * @brief High-performance YAML and JSON parser and emitter for Ksee.
 */

#ifndef KSEE_FORMAT_YAML_HPP
#define KSEE_FORMAT_YAML_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Tree.hpp"

namespace Ksee {

class Yaml {
public:
    static bool parse(const String &yamlString, Tree<void> &outRoot);

    static String toYAML(const Tree<void> &root, int indentation = 2);
    static String toJSON(const Tree<void> &root, int indentation = 4);
};

inline bool parseYAML(const String &yamlString, Tree<void> &tree) {
    return Yaml::parse(yamlString, tree);
}

inline bool parseJSON(const String &jsonString, Tree<void> &tree) {
    return Yaml::parse(jsonString, tree);
}

inline String toYAML(const Tree<void> &tree, int indentation = 2) {
    return Yaml::toYAML(tree, indentation);
}

inline String toJSON(const Tree<void> &tree, int indentation = 4) {
    return Yaml::toJSON(tree, indentation);
}

} // namespace Ksee

#endif // KSEE_FORMAT_YAML_HPP

