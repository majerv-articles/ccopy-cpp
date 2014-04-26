set(LLVM_USED_LIBS clangTooling clangBasic clangAST)

add_clang_executable(ccopy CCopy.cpp CopyGroups.cpp)

target_link_libraries(ccopy
  clangTooling
  clangBasic
  clangASTMatchers
  )

