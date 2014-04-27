#include <fstream>
#include <string>

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"

#include "CopyGroups.h"

using namespace clang;

std::string readFileContent(const char* fileName) {
    std::ifstream file(fileName);
    if (file.is_open())
    {
        return std::string( (std::istreambuf_iterator<char>(file) ),
                            (std::istreambuf_iterator<char>()) );
    }
    else {
        llvm::errs() << "Unable to open file";
        // FIXME use proper exception 
        return "";
    }
}

class CCopyConstructorInjector { 
public:
    CCopyConstructorInjector(CompilerInstance* compiler);
    void inject(CXXRecordDecl* recordDecl);
    void dump(const llvm::StringRef& inFile);
    
private:
    CompilerInstance* compiler;
    Rewriter rewriter;
};

CCopyConstructorInjector::CCopyConstructorInjector(CompilerInstance* compilerInstance): compiler(compilerInstance) {
    rewriter.setSourceMgr(compiler->getSourceManager(), compiler->getLangOpts());
}

void CCopyConstructorInjector::inject(CXXRecordDecl*const recordDecl) {
    const SourceRange sourceRange = recordDecl->getSourceRange();
    const SourceLocation sourceLocation= sourceRange.getEnd();
    
    const std::string className = recordDecl->getNameAsString();
    
    ccopy::CopyGroups copygroups;
    copygroups.divideBy(recordDecl->field_begin(), recordDecl->field_end());
    
    if (copygroups.size() < 2) {
        llvm::outs() << "skipping code generation: less than 2 copy groups, default copy constructor fits" << "\n";
        rewriter.InsertTextAfter(sourceLocation, "//less than 2 copy groups, default copy constructor fits\n");
        return;
    } 
    
    rewriter.InsertTextAfter(sourceLocation, "\npublic:\n"+ className + "(const " + className + "& other) {\n");

    std::string joins;
    for(auto cit = copygroups.begin(); cit != copygroups.end(); ++cit) {
        ccopy::CopyGroups::group_type group = cit->second;

        rewriter.InsertText(sourceLocation, "std::thread " + cit->first + "([&](){\n", true, true);

        for(auto git = group.begin(); git != group.end(); ++git) {
            std::string fieldName = *git;
            rewriter.InsertText(sourceLocation, fieldName + " = other." +  fieldName + ";\n", true, true);
        }
        
        rewriter.InsertText(sourceLocation, "});\n\n", true, true);
        
        joins += cit->first + ".join()\n";
    }
    
    rewriter.InsertText(sourceLocation, joins, true, true);
    
    rewriter.InsertTextAfter(sourceLocation, "}\n\n");
}

void CCopyConstructorInjector::dump(const llvm::StringRef& inFile){
    std::string outName(inFile.data()); //compiler->getSourceManager().getFilename(sourceLocation);
    llvm::outs() << "Source file: " << outName << "\n";
    
    size_t ext = outName.rfind(".");
    if (ext == std::string::npos)
        ext = outName.length();
  
    outName.insert(ext, "_out");
    llvm::outs() << "Generating output to: " << outName << "\n";
    std::string OutErrorInfo;
    llvm::raw_fd_ostream outFile(outName.c_str(), OutErrorInfo, llvm::sys::fs::F_None);

    if (OutErrorInfo.empty())
    {
        // Output some #ifdefs
        outFile << "#define AUTHOR majerv\n";
        outFile << "#define PROGRAM ccopy\n";

        // Now output rewritten source code
        const RewriteBuffer *RewriteBuf = rewriter.getRewriteBufferFor(compiler->getSourceManager().getMainFileID());
        const std::string content(RewriteBuf->begin(), RewriteBuf->end());
    
        //llvm::outs() << content;
        outFile << std::string(RewriteBuf->begin(), RewriteBuf->end());    
    }
    else
    {
        llvm::errs() << "Cannot open " << outName << " for writing\n";
    }

    outFile.close();
}

class FindExpensiveClassVisitor
  : public RecursiveASTVisitor<FindExpensiveClassVisitor> {
public:
  explicit FindExpensiveClassVisitor(CCopyConstructorInjector* injector)
    : injector(injector) {}

  bool VisitCXXRecordDecl(CXXRecordDecl* recordDecl) {
    const bool hasCopyCtr = recordDecl->hasUserDeclaredCopyConstructor();

    if (hasCopyCtr) {
        llvm::outs() << "copy constructor found for " << recordDecl->getNameAsString() << ", skipping code generation\n";
    }
    else if (recordDecl->isCompleteDefinition()) {
        if (isC4(recordDecl)) {
            llvm::outs() << "generating copy constructor for " << recordDecl->getNameAsString() << "\n";
            injector->inject(recordDecl);
        }
        else {
            llvm::outs() << "no marker on " << recordDecl->getNameAsString() << ", skipping code generation\n";
        }
    }

    return true;
  }

private:
  CCopyConstructorInjector*const injector;
  
  bool isC4(const CXXRecordDecl*const recordDecl) {
    // FIXME make this work
    //return recordDecl->isDerivedFrom(recordDeclOfC4);
    
    for(auto cit = recordDecl->bases_begin(); cit != recordDecl->bases_end(); ++cit) {
        if (cit->getType().getAsString() == "class C4") {
            return true;
        };
    }
    return false;
  }
  
};

class FindExpensiveClassConsumer : public clang::ASTConsumer {
public:
  explicit FindExpensiveClassConsumer(clang::CompilerInstance* compiler, llvm::StringRef inFile)
    : inFile(inFile), injector(compiler), visitor(&injector) {}

    virtual void HandleTranslationUnit(clang::ASTContext& context) {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
        injector.dump(inFile);
    }
private:
  llvm::StringRef inFile;
  CCopyConstructorInjector injector;
  FindExpensiveClassVisitor visitor;
};

class FindExpensiveClassAction : public clang::ASTFrontendAction {
public:
  virtual clang::ASTConsumer* CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef inFile) {
    return new FindExpensiveClassConsumer(&compiler, inFile);
  }
};

int main(int argc, char **argv) {
  if (argc < 2) {
    llvm::errs() << "Not enough arguments.\n";
    return 1;
  }
  
  const char* fileName = argv[1];
  clang::tooling::runToolOnCode(new FindExpensiveClassAction, readFileContent(fileName), fileName);
    
  return 0;
}

