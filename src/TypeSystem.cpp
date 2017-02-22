#include "TypeSystem.h"

#include <iostream>
#include <cctype>

#include "NotImplementedException.h"
#include "Tables.h"
#include "SchemaParser.h"

namespace tigl {
    namespace {
        auto makeClassName(std::string name) -> std::string {
            if (!name.empty()) {
                // capitalize first letter
                name[0] = std::toupper(name[0]);

                // strip Type suffix if exists
                name = stripTypeSuffix(name);

                // prefix CPACS
                name = "CPACS" + name;
            }

            return name;
        }

        auto resolveType(const SchemaParser& schema, const std::string& name, const Tables& tables) -> std::string {
            // search simple and complex types
            const auto& types = schema.types();
            const auto cit = types.find(name);
            if (cit != std::end(types)) {
                const auto p = tables.m_typeSubstitutions.find(name);
                if (p)
                    return *p;
                else
                    return makeClassName(name);
            }

            // search predefined xml schema types and replace them
            const auto p = tables.m_typeSubstitutions.find(name);
            if (p)
                return *p;

            throw std::runtime_error("Unknown type: " + name);
        }

        auto buildFieldList(const SchemaParser& schema, const ComplexType& type, const Tables& tables) -> std::vector<Field> {
            std::vector<Field> members;

            // attributes
            for (const auto& a : type.attributes) {
                Field m;
                m.origin = &a;
                m.cpacsName = a.name;
                m.typeName = resolveType(schema, a.type, tables);
                m.xmlType = XMLConstruct::Attribute;
                if (a.optional)
                    m.cardinality = Cardinality::Optional;
                else
                    m.cardinality = Cardinality::Mandatory;
                members.push_back(m);
            }

            // elements
            struct ContentVisitor : public boost::static_visitor<> {
                ContentVisitor(const SchemaParser& schema, std::vector<Field>& members, const Tables& tables)
                    : schema(schema), members(members), tables(tables) {}

                void operator()(const Element& e) const {
                    Field m;
                    m.origin = &e;
                    m.cpacsName = e.name;
                    m.typeName = resolveType(schema, e.type, tables);
                    m.xmlType = XMLConstruct::Element;
                    if (e.minOccurs == 0 && e.maxOccurs == 1)
                        m.cardinality = Cardinality::Optional;
                    else if (e.minOccurs == 1 && e.maxOccurs == 1)
                        m.cardinality = Cardinality::Mandatory;
                    else if (e.minOccurs >= 0 && e.maxOccurs > 1)
                        m.cardinality = Cardinality::Vector;
                    else if (e.minOccurs == 0 && e.maxOccurs == 0) {
                        std::cerr << "Warning: Element " + e.name + " with type " + e.type + " was omitted as minOccurs and maxOccurs are both zero" << std::endl;
                        return; // skip this type
                    } else
                        throw std::runtime_error("Invalid cardinalities, min: " + std::to_string(e.minOccurs) + ", max: " + std::to_string(e.maxOccurs));
                    members.push_back(m);
                }

                void operator()(const Choice& c) const {
                    unsigned int choiceIndex = 1;
                    std::vector<Field> allChoiceMembers;
                    for (const auto& v : c.elements) {
                        // collect members of one choice
                        std::vector<Field> choiceMembers;
                        v.visit(ContentVisitor(schema, choiceMembers, tables));

                        // make all optional
                        for (auto& f : choiceMembers)
                            if (f.cardinality == Cardinality::Mandatory)
                                f.cardinality = Cardinality::Optional;

                        // give custom names
                        for (auto& f : choiceMembers)
                            f.customFieldName = f.cpacsName + "_choice" + std::to_string(choiceIndex);

                        // copy to output
                        std::copy(std::begin(choiceMembers), std::end(choiceMembers), std::back_inserter(allChoiceMembers));

                        choiceIndex++;
                    }

                    // consistency check, two types with the same name but different types or cardinality are problematic
                    for (std::size_t i = 0; i < allChoiceMembers.size(); i++) {
                        const auto& f1 = allChoiceMembers[i];
                        for (std::size_t j = i + 1; j < allChoiceMembers.size(); j++) {
                            const auto& f2 = allChoiceMembers[j];
                            if (f1.cpacsName == f2.cpacsName && (f1.cardinality != f2.cardinality || f1.typeName != f2.typeName)) {
                                std::cerr << "Elements with same name but different cardinality or type inside choice" << std::endl;
                                for (const auto& f : { f1, f2 })
                                    std::cerr << f.cpacsName << " " << toString(f.cardinality) << " " << f.typeName << std::endl;
                            }
                        }
                    }

                    // copy to output
                    std::copy(std::begin(allChoiceMembers), std::end(allChoiceMembers), std::back_inserter(members));
                }

                void operator()(const Sequence& s) const {
                    for (const auto& v : s.elements)
                        v.visit(ContentVisitor(schema, members, tables));
                }

                void operator()(const All& a) const {
                    for (const auto& e : a.elements)
                        operator()(e);
                }

                void operator()(const Any& a) const {
                    throw NotImplementedException("Generating fields for any is not implemented");
                }

                void operator()(const Group& g) const {
                    throw NotImplementedException("Generating fields for group is not implemented");
                }

                void operator()(const SimpleContent& g) const {
                    Field m;
                    m.origin = &g;
                    m.cpacsName = "";
                    m.customFieldName = "simpleContent";
                    m.cardinality = Cardinality::Mandatory;
                    m.typeName = resolveType(schema, g.type, tables);
                    m.xmlType = XMLConstruct::SimpleContent;
                    members.push_back(m);
                }

            private:
                const SchemaParser& schema;
                std::vector<Field>& members;
                const Tables& tables;
            };

            type.content.visit(ContentVisitor(schema, members, tables));

            return members;
        }
    }

    TypeSystem::TypeSystem(const SchemaParser& schema, const Tables& tables)
        : tables(tables) {
        std::cout << "Creating type system" << std::endl;

        for (const auto& p : schema.types()) {
            const auto& type = p.second;

            struct TypeVisitor {
                TypeVisitor(const SchemaParser& schema, TypeSystem& types, const Tables& tables)
                    : schema(schema), types(types), tables(tables) {}

                void operator()(const ComplexType& type) {
                    Class c;
                    c.origin = &type;
                    c.name = makeClassName(type.name);
                    c.fields = buildFieldList(schema, type, tables);
                    if (!type.base.empty()) {
                        c.base = resolveType(schema, type.base, tables);

                        // make base a field if fundamental type
                        if (tables.m_fundamentalTypes.contains(c.base)) {
                            std::cout << "Warning: Type " << type.name << " has base class " << c.base << " which is a fundamental type. Generated field 'base' instead" << std::endl;

                            Field f;
                            f.cpacsName = "";
                            f.customFieldName = "base";
                            f.cardinality = Cardinality::Mandatory;
                            f.typeName = c.base;
                            f.xmlType = XMLConstruct::FundamentalTypeBase;

                            c.fields.insert(std::begin(c.fields), f);
                            c.base.clear();
                        }
                    }

                    types.classes[c.name] = c;
                }

                void operator()(const SimpleType& type) {
                    if (type.restrictionValues.size() > 0) {
                        // create enum
                        Enum e;
                        e.origin = &type;
                        e.name = makeClassName(type.name);
                        for (const auto& v : type.restrictionValues)
                            e.values.push_back(EnumValue(v));
                        types.enums[e.name] = e;
                    } else
                        throw NotImplementedException("Simple times which are not enums are not implemented");
                }

            private:
                const SchemaParser& schema;
                TypeSystem& types;
                const Tables& tables;
            };

            type.visit(TypeVisitor(schema, *this, tables));
        };
    }

    namespace {
        // TODO: replace by lambda when C++14 is available
        struct SortAndUnique {
            template <typename T>
            void operator()(std::vector<T*>& con) {
                std::sort(std::begin(con), std::end(con), [](const T* a, const T* b) {
                    return a->name < b->name;
                });
                con.erase(std::unique(std::begin(con), std::end(con), [](const T* a, const T* b) {
                    return a->name == b->name;
                }), std::end(con));
            }
        };
    }

    void TypeSystem::buildDependencies(const std::unordered_map<std::string, std::string>& replacedEnums) {
        std::cout << "Building dependencies" << std::endl;

        for (auto& p : classes) {
            auto& c = p.second;

            // base
            if (!c.base.empty()) {
                const auto it = classes.find(c.base);
                if (it != std::end(classes)) {
                    c.deps.bases.push_back(&it->second);
                    it->second.deps.deriveds.push_back(&c);
                } else
                    // this exception should be prevented by earlier code
                    throw std::runtime_error("Class " + c.name + " has non-class base: " + c.base);
            }

            // fields
            for (auto& f : c.fields) {
                // check if this type has been replaced
                const auto& rit = replacedEnums.find(f.typeName);
                if (rit != std::end(replacedEnums))
                    f.typeName = rit->second;

                const auto eit = enums.find(f.typeName);
                if (eit != std::end(enums)) {
                    c.deps.enumChildren.push_back(&eit->second);
                    eit->second.deps.parents.push_back(&c);
                } else {
                    const auto cit = classes.find(f.typeName);
                    if (cit != std::end(classes)) {
                        c.deps.children.push_back(&cit->second);
                        cit->second.deps.parents.push_back(&c);
                    }
                }
            }
        }

        SortAndUnique sortAndUnique;

        // sort and unique
        for (auto& p : classes) {
            auto& c = p.second;
            sortAndUnique(c.deps.bases);
            sortAndUnique(c.deps.children);
            sortAndUnique(c.deps.deriveds);
            sortAndUnique(c.deps.enumChildren);
            sortAndUnique(c.deps.parents);
        }

        for (auto& p : enums) {
            auto& e = p.second;
            sortAndUnique(e.deps.parents);
        }
    }

    auto TypeSystem::collapseEnums() -> std::unordered_map<std::string, std::string> {
        std::cout << "Collapsing enums" << std::endl;

        // convert enum unordered_map to vector for easier processing
        std::vector<Enum> enumVec;
        enumVec.reserve(enums.size());
        for (const auto& p : enums)
            enumVec.push_back(p.second);

        std::unordered_map<std::string, std::string> replacedEnums;

        for (std::size_t i = 0; i < enumVec.size(); i++) {
            auto& e1 = enumVec[i];
            for (std::size_t j = i + 1; j < enumVec.size(); j++) {
                auto& e2 = enumVec[j];

                auto stripNumber = [](std::string name) {
                    // handle inline enum types
                    // generated names are of the form: <containing type name>_<element name>[_SimpleContent][_<counter>]

                    // remove optional digits and underscore at the end
                    while (!name.empty() && std::isdigit(name.back()))
                        name.pop_back();
                    if (name.back() == '_')
                        name.erase(name.length() - 1);
                    
                    // remove _SimpleContent
                    name = stripSimpleContentSuffix(name);

                    // if type contains an underscore, remove preceding part
                    const auto& pos = name.find_last_of('_');
                    if (pos != std::string::npos)
                        name.erase(0, pos + 1);

                    // capitalize first letter
                    name[0] = std::toupper(name[0]);

                    // strip Type suffix if exists
                    name = stripTypeSuffix(name);

                    // prefix CPACS if not exists
                    if (name.compare(0, 5, "CPACS") != 0)
                        name = "CPACS" + name;

                    return name;
                };

                if (e1.values.size() == e2.values.size()) {
                    // strip name decorators
                    const auto& e1StrippedName = stripNumber(e1.name);
                    const auto& e2StrippedName = stripNumber(e2.name);
                    if (e1StrippedName == e2StrippedName && std::equal(std::begin(e1.values), std::end(e1.values), std::begin(e2.values))) {
                        // choose new name
                        const auto newName = [&] {
                            // if the stripped name is not already taken, use it. Otherwise, take the shorter of the two enum names
                            if (classes.count(e1StrippedName) == 0 && enums.count(e1StrippedName) == 0)
                                return e1StrippedName;
                            else
                                return std::min(e1.name, e2.name);
                        }();

                        // register replacements
                        if (e1.name != newName) replacedEnums[e1.name] = newName;
                        if (e2.name != newName) replacedEnums[e2.name] = newName;

                        std::cout << "Collapsed enums " << e1.name << " and " << e2.name << " to " << newName << std::endl;

                        // remove e2 and rename e1
                        enumVec.erase(std::begin(enumVec) + j);
                        e1.name = newName;
                    }
                }
            }
        }

        // fill enum back again from vector
        enums.clear();
        for (auto& e : enumVec)
            enums[e.name] = std::move(e);

        return replacedEnums;
    }

    namespace {
        void includeNode(Enum& e, const Table& pruneList) {
            // if this enum is already marked, return
            if (e.pruned == false)
                return;

            // if this enum is on the prune list, just leave it
            if (pruneList.contains(e.name))
                return;

            // enum is not pruned, mark it
            e.pruned = false;
        }

        void includeNode(Class& cls, const Table& pruneList) {
            // if this class is already marked, return
            if (cls.pruned == false)
                return;

            // if this class is on the prune list, just leave it and all its sub element types pruned
            if (pruneList.contains(cls.name))
                return;

            // class is not pruned, mark it
            cls.pruned = false;

            auto& deps = cls.deps;

            // try to include all bases
            for (auto& b : deps.bases)
                includeNode(*b, pruneList);

            // try to include all field classes
            for (auto& c : deps.children)
                includeNode(*c, pruneList);

            // try to include all field enums
            for (auto& e : deps.enumChildren)
                includeNode(*e, pruneList);
        }
    }

    void TypeSystem::runPruneList() {
        std::cout << "Running prune list" << std::endl;

        // mark all types as pruned
        for (auto& p : classes)
            p.second.pruned = true;
        for (auto& p : enums)
            p.second.pruned = true;

        // find root type
        const auto rootElementTypeName = std::string("CPACSCpacs");
        const auto it = classes.find(rootElementTypeName);
        if (it == std::end(classes))
            throw std::runtime_error("Could not find root element. Expected: " + rootElementTypeName);
        auto& root = it->second;

        includeNode(root, tables.m_pruneList);

        std::cout << "The following types have been pruned:" << std::endl;
        std::vector<std::string> prunedTypeNames;
        for (const auto& p : classes)
            if(p.second.pruned)
                prunedTypeNames.push_back("Class: " + p.second.name);
        for (const auto& p : enums)
            if (p.second.pruned)
                prunedTypeNames.push_back("Enum: " + p.second.name);
        std::sort(std::begin(prunedTypeNames), std::end(prunedTypeNames));
        for(const auto& name : prunedTypeNames)
            std::cout << "\t" << name << std::endl;

        // remove pruned classes from fields and bases
        auto isPruned = [&](const std::string& name) {
            const auto& cit = classes.find(name);
            if (cit != std::end(classes) && cit->second.pruned)
                return true;
            const auto& eit = enums.find(name);
            if (eit != std::end(enums) && eit->second.pruned)
                return true;
            return false;
        };

        for (auto& c : classes) {
            auto& fields = c.second.fields;
            fields.erase(std::remove_if(std::begin(fields), std::end(fields), [&](const Field& f) {
                return isPruned(f.typeName);
            }), std::end(fields));

            auto& baseName = c.second.base;
            if (isPruned(baseName))
                baseName.clear();
        }
    }

    auto buildTypeSystem(const SchemaParser& schema, const Tables& tables) -> TypeSystem {
        TypeSystem typeSystem(schema, tables);
        const auto& replacedEnums = typeSystem.collapseEnums();
        typeSystem.buildDependencies(replacedEnums);
        typeSystem.runPruneList();
        return typeSystem;
    }
}
