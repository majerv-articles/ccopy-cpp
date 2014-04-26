#ifndef COPY_GROUPS_H
#define COPY_GROUPS_H

#include "clang/AST/Decl.h"

#include <string>
#include <map>
#include <set>

namespace ccopy {

    class CopyGroups {
    public:
        typedef std::set<std::string> group_type;
        typedef std::map<std::string, group_type>::const_iterator const_iterator_type;
    
        void divideBy(clang::RecordDecl::field_iterator fields_begin, clang::RecordDecl::field_iterator fields_end);
        size_t size() const;

        const_iterator_type begin() { return groups.begin(); }
        const_iterator_type end() { return groups.end(); }
        
    private:
        std::map<std::string, group_type> groups;
    };

}
#endif //COPY_GROUPS_H
