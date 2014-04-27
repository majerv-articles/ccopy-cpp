#include "CopyGroups.h"

using namespace clang;

static std::string DEFAULT_GROUP_NAME = "default_group";
static std::string C4_GROUP_NAME = "c4_fields";

void ccopy::CopyGroups::divideBy(RecordDecl::field_iterator fields_begin, RecordDecl::field_iterator fields_end) {
    std::string groupName;
    
    for( RecordDecl::field_iterator fit = fields_begin; fit != fields_end; ++fit ) {
        groupName = determineGroupOf(*fit);
        groups[groupName].insert((*fit)->getDeclName().getAsString());
    }
}

size_t ccopy::CopyGroups::size() const {
    return groups.size();
}

std::string ccopy::CopyGroups::determineGroupOf(FieldDecl*const fieldDecl) {
    const QualType qualType = fieldDecl->getType();
    const Type*const type = qualType.getTypePtr();
    
    if(type->isUnionType() || type->isEnumeralType() || type->isPointerType()) {
        return DEFAULT_GROUP_NAME;
    }
    else if(type->isRecordType() /*&& isC4*/) {
        return C4_GROUP_NAME;
    }
        
    return DEFAULT_GROUP_NAME;
}

