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
    void dump();
    
private:
    CompilerInstance* compiler;
    Rewriter rewriter;
};

CCopyConstructorInjector::CCopyConstructorInjector(CompilerInstance* compilerInstance): compiler(compilerInstance) {
    rewriter.setSourceMgr(compiler->getSourceManager(), compiler->getLangOpts());
}

void CCopyConstructorInjector::inject(CXXRecordDecl* recordDecl) {
    SourceRange sourceRange = recordDecl->getSourceRange();
    SourceLocation sourceLocation= sourceRange.getEnd();
    
    //std::string outName = compiler->getSourceManager().getFilename(sourceLocation);
    //llvm::errs() << outName  << "\n";
    
    const std::string className = recordDecl->getNameAsString();
    
    rewriter.InsertTextAfter(sourceLocation, "\npublic:\n"+ className + "(const " + className + "& other) {\n");
    
    for( RecordDecl::field_iterator fit = recordDecl->field_begin(); fit != recordDecl->field_end(); ++fit ) {
        std::string fieldName = (*fit)->getDeclName().getAsString();
        rewriter.InsertText(sourceLocation, "    " + fieldName + " = other." +  fieldName + ";\n", true, true);
    }
    
    rewriter.InsertTextAfter(sourceLocation, "}\n\n");
}

void CCopyConstructorInjector::dump(){
    std::string outName = "rc.cpp"; //compiler->getSourceManager().getFilename(sourceLocation);
    llvm::outs() << "Translation unit: " << outName << "\n";
    
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
  explicit FindExpensiveClassVisitor(CompilerInstance* compiler, CCopyConstructorInjector* injector)
    : injector(injector) {}

  bool VisitCXXRecordDecl(CXXRecordDecl* recordDecl) {
    const bool hasCopyCtr = recordDecl->hasUserDeclaredCopyConstructor();

    if (recordDecl->hasTrivialCopyConstructor() && recordDecl->isCompleteDefinition()) {
        llvm::outs() << "trivial copy constructor found for " << recordDecl->getNameAsString() << ", skipping code generation\n";
    }

    if (!hasCopyCtr && recordDecl->isCompleteDefinition()) {
        injector->inject(recordDecl);
    }

    return true;
  }

private:
  CCopyConstructorInjector*const injector;
};

class FindExpensiveClassConsumer : public clang::ASTConsumer {
public:
  explicit FindExpensiveClassConsumer(CompilerInstance* compiler)
    : injector(compiler), visitor(compiler, &injector) {}

    virtual void HandleTranslationUnit(clang::ASTContext& context) {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
        injector.dump();
    }
private:
  CCopyConstructorInjector injector;
  FindExpensiveClassVisitor visitor;
};

class FindExpensiveClassAction : public clang::ASTFrontendAction {
public:
  virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    return new FindExpensiveClassConsumer(&Compiler);
  }
};

int main(int argc, char **argv) {
  if (argc < 2) {
    llvm::errs() << "Not enough arguments.\n";
    return 1;    l
  }
  
  const char* fileName = argv[1];
  clang::tooling::runToolOnCode(new FindExpensiveClassAction, readFileContent(fileName), fileName);
    
  return 0;
}

