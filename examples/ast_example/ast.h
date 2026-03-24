#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>

// 全局节点ID计数器，用于生成唯一的节点标识
static int node_id_counter = 0;

// AST节点基类
class BaseAST {
public:
    virtual ~BaseAST() = default;
    
    virtual std::string DumpDOT() const = 0;

    virtual int GetNodeID() const = 0;
    
    static int NewNodeID() {
        return node_id_counter++;
    }
};

// 类型枚举
enum Type {
    TYPE_INT,
    TYPE_VOID
};

// 编译单元AST节点
class CompUnitAST : public BaseAST {
private:
    int node_id;

public:
    std::unique_ptr<BaseAST> decl;
    std::unique_ptr<BaseAST> main_func_def;

    CompUnitAST() : node_id(NewNodeID()) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        // 定义此节点
        ss << "  node" << node_id << " [label=\"CompUnit\", shape=box, style=filled, fillcolor=lightblue];\n";
        
        // 连接子节点
        if (decl) {
            ss << "  node" << node_id << " -> node" << decl->GetNodeID() 
               << " [label=\"decl\"];\n";
            ss << decl->DumpDOT();
        }
        
        if (main_func_def) {
            ss << "  node" << node_id << " -> node" << main_func_def->GetNodeID() 
               << " [label=\"main_func_def\"];\n";
            ss << main_func_def->DumpDOT();
        }
        
        return ss.str();
    }
};

// 声明AST节点
class DeclAST : public BaseAST {
private:
    int node_id;

public:
    enum DeclKind {
        CONST_DECL,
        VAR_DECL
    };

    DeclKind kind;
    std::unique_ptr<BaseAST> decl;  // 可以是 ConstDecl 或 VarDecl

    DeclAST() : node_id(NewNodeID()) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        ss << "  node" << node_id << " [label=\"Decl\", shape=box];\n";
        
        if (decl) {
            ss << "  node" << node_id << " -> node" << decl->GetNodeID() 
               << " [label=\"decl\"];\n";
            ss << decl->DumpDOT();
        }
        
        return ss.str();
    }
};

// 常量声明AST节点
class ConstDeclAST : public BaseAST {
private:
    int node_id;

public:
    Type type;
    std::string ident;
    int value;

    ConstDeclAST() : node_id(NewNodeID()) {}
    
    // 从C风格字符串构造
    ConstDeclAST(const char* id) : node_id(NewNodeID()), ident(id) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        std::string label = "ConstDecl\\nident: " + ident + 
                            "\\ntype: " + (type == TYPE_INT ? "int" : "void") +
                            "\\nvalue: " + std::to_string(value);
        
        ss << "  node" << node_id << " [label=\"" << label << "\", shape=box, style=filled, fillcolor=lightyellow];\n";
        
        return ss.str();
    }
};

// 变量声明AST节点
class VarDeclAST : public BaseAST {
private:
    int node_id;

public:
    Type type;
    std::string ident;

    VarDeclAST() : node_id(NewNodeID()) {}
    
    // 从C风格字符串构造
    VarDeclAST(const char* id) : node_id(NewNodeID()), ident(id) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        std::string label = "VarDecl\\nident: " + ident + 
                            "\\ntype: " + (type == TYPE_INT ? "int" : "void");
        
        ss << "  node" << node_id << " [label=\"" << label << "\", shape=box, style=filled, fillcolor=lightgreen];\n";
        
        return ss.str();
    }
};

// 主函数定义AST节点
class MainFuncDefAST : public BaseAST {
private:
    int node_id;

public:
    std::unique_ptr<BaseAST> block;

    MainFuncDefAST() : node_id(NewNodeID()) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        ss << "  node" << node_id << " [label=\"MainFuncDef\\nint main()\", shape=box, style=filled, fillcolor=lightcoral];\n";
        
        if (block) {
            ss << "  node" << node_id << " -> node" << block->GetNodeID() 
               << " [label=\"block\"];\n";
            ss << block->DumpDOT();
        }
        
        return ss.str();
    }
};

// 代码块AST节点
class BlockAST : public BaseAST {
private:
    int node_id;

public:
    std::vector<std::unique_ptr<BaseAST>> items;

    BlockAST() : node_id(NewNodeID()) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        ss << "  node" << node_id << " [label=\"Block\", shape=box, style=filled, fillcolor=lightgrey];\n";
        
        // 连接所有子项目
        for (size_t i = 0; i < items.size(); i++) {
            if (items[i]) {
                ss << "  node" << node_id << " -> node" << items[i]->GetNodeID() 
                   << " [label=\"item" << i << "\"];\n";
                ss << items[i]->DumpDOT();
            }
        }
        
        return ss.str();
    }
};

// 块项目AST节点
class BlockItemAST : public BaseAST {
private:
    int node_id;

public:
    std::unique_ptr<BaseAST> item;  // 可以是 Decl 或 Stmt

    BlockItemAST() : node_id(NewNodeID()) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        ss << "  node" << node_id << " [label=\"BlockItem\", shape=box];\n";
        
        if (item) {
            ss << "  node" << node_id << " -> node" << item->GetNodeID() 
               << " [label=\"item\"];\n";
            ss << item->DumpDOT();
        }
        
        return ss.str();
    }
};

// 语句AST节点
class StmtAST : public BaseAST {
private:
    int node_id;

public:
    enum StmtKind {
        ASSIGNMENT,
        BLOCK,
        RETURN
    };

    StmtKind kind;
    std::unique_ptr<BaseAST> content;  // Assignment, Block 或 返回值标识符
    std::string return_ident;  // 如果是RETURN类型，存储返回的标识符

    StmtAST() : node_id(NewNodeID()) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        std::string label;
        
        switch (kind) {
            case ASSIGNMENT:
                label = "Stmt\\nkind: ASSIGNMENT";
                break;
            case BLOCK:
                label = "Stmt\\nkind: BLOCK";
                break;
            case RETURN:
                label = "Stmt\\nkind: RETURN\\nident: " + return_ident;
                break;
        }
        
        ss << "  node" << node_id << " [label=\"" << label << "\", shape=box, style=filled, fillcolor=lightskyblue];\n";
        
        if (content) {
            std::string edge_label;
            switch (kind) {
                case ASSIGNMENT:
                    edge_label = "assignment";
                    break;
                case BLOCK:
                    edge_label = "block";
                    break;
                default:
                    edge_label = "content";
                    break;
            }
            
            ss << "  node" << node_id << " -> node" << content->GetNodeID() 
               << " [label=\"" << edge_label << "\"];\n";
            ss << content->DumpDOT();
        }
        
        return ss.str();
    }
};

// 赋值语句AST节点
class AssignmentAST : public BaseAST {
private:
    int node_id;

public:
    std::string ident;

    AssignmentAST() : node_id(NewNodeID()) {}
    
    // 从C风格字符串构造
    AssignmentAST(const char* id) : node_id(NewNodeID()), ident(id) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        std::string label = "Assignment\\nident: " + ident + "\\nis_getint: true";
        
        ss << "  node" << node_id << " [label=\"" << label << "\", shape=box, style=filled, fillcolor=lightsalmon];\n";
        
        return ss.str();
    }
};

// 数字AST节点
class NumberAST : public BaseAST {
private:
    int node_id;

public:
    int value;

    NumberAST() : node_id(NewNodeID()) {}

    NumberAST(int val) : node_id(NewNodeID()), value(val) {}

    int GetNodeID() const override {
        return node_id;
    }

    std::string DumpDOT() const override {
        std::stringstream ss;
        
        std::string label = "Number\\nvalue: " + std::to_string(value);
        
        ss << "  node" << node_id << " [label=\"" << label << "\", shape=ellipse, style=filled, fillcolor=lightpink];\n";
        
        return ss.str();
    }
};


