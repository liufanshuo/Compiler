#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <memory>
#include <fstream>
#include <string>
#include "ast.h"

// 声明外部变量和函数
extern FILE* yyin;
extern int yyparse();
extern int yydebug;
extern std::unique_ptr<BaseAST> root;

// 生成并输出DOT格式的AST图
void dumpDOTGraph(const std::string& output_file) {
    std::ofstream out;
    
    // 如果提供了输出文件名，打开文件
    if (!output_file.empty()) {
        out.open(output_file);
        if (!out.is_open()) {
            std::cerr << "Error: Cannot open output file: " << output_file << std::endl;
            return;
        }
    }
    
    // 选择输出流
    std::ostream& os = output_file.empty() ? std::cout : out;
    
    // 输出DOT格式的头部
    os << "digraph AST {\n";
    os << "  node [fontname=\"Arial\"];\n";
    
    // 输出AST节点和边
    if (root) {
        os << root->DumpDOT();
    } else {
        os << "  node0 [label=\"Error: AST root is null!\", color=red];\n";
    }
    
    // 输出DOT格式的尾部
    os << "}\n";
    
    // 关闭文件
    if (out.is_open()) {
        out.close();
    }
}

int main(int argc, char* argv[]) {
    // 处理命令行参数
    bool debug_mode = false;
    std::string input_file;
    std::string output_file;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--debug") {
            debug_mode = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg[0] != '-') {
            input_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            std::cerr << "Usage: " << argv[0] << " [-d|--debug] [-o output_file] [input_file]" << std::endl;
            return 1;
        }
    }
    
    // 设置调试模式
    yydebug = debug_mode ? 1 : 0;
    
    // 设置输入文件
    if (!input_file.empty()) {
        yyin = fopen(input_file.c_str(), "r");
        if (!yyin) {
            std::cerr << "Cannot open input file: " << input_file << std::endl;
            return 1;
        }
    } else {
        yyin = stdin;
    }
    
    // 进行语法分析并构建AST
    int parse_result = yyparse();
    
    // 关闭输入文件
    if (yyin != stdin) {
        fclose(yyin);
    }
    
    // 检查解析结果
    if (parse_result != 0) {
        std::cerr << "Parsing failed with error code: " << parse_result << std::endl;
        return 1;
    }
    
    // 生成DOT格式的AST图
    // std::cout << "Parsing completed successfully. Generating DOT graph..." << std::endl;
    dumpDOTGraph(output_file);
    
    // 如果输出到文件，打印使用说明
    if (!output_file.empty()) {
        std::cout << "DOT graph saved to " << output_file << std::endl;
        std::cout << "To generate a visual graph, run:" << std::endl;
        std::cout << "  dot -Tpng " << output_file << " -o ast.png" << std::endl;
    }
    
    return 0;
}
