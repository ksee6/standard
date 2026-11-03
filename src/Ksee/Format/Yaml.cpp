/**
 * @file Yaml.cpp
 * @brief High-performance YAML and JSON parser and emitter implementation delegating to Tree core.
 */

#include "../../../include/Ksee/Format/Yaml.hpp"

namespace Ksee {

bool Yaml::parse(const String &yamlString, Tree<void> &outRoot) {
    return parseTree(yamlString, outRoot);
}

String Yaml::toYAML(const Tree<void> &root, int indentation) {
    return stringifyYaml(root, indentation >= 0 ? (u32)indentation : 2);
}

String Yaml::toJSON(const Tree<void> &root, int indentation) {
    return stringifyJSON(root, indentation >= 0 ? (u32)indentation : 0);
}

} // namespace Ksee
