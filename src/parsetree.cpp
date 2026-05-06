#include "parsetree.hpp"

// Helper rekursif.WW
static void printTreeHelper(const std::shared_ptr<ParseNode> &node,
                            const std::string &prefix,
                            bool isLast,
                            std::ostream &out)
{
    if (!node)
        return;

    // Connector ke node ini: └── kalau anak terakhir, ├── kalau bukan
    out << prefix
        << (isLast ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 "  // └──
                   : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ") // ├──
        << node->label << "\n";

    // node anak terakhir -> "    " 
    // bukan -> "│   " 
    std::string childPrefix = prefix + (isLast ? "    "
                                               : "\xe2\x94\x82   "); // │

    for (size_t i = 0; i < node->children.size(); ++i)
    {
        bool childIsLast = (i == node->children.size() - 1);
        printTreeHelper(node->children[i], childPrefix, childIsLast, out);
    }
}

void printTree(const std::shared_ptr<ParseNode> &root, std::ostream &out)
{
    if (!root)
    {
        out << "(empty tree)\n";
        return;
    }

    out << root->label << "\n";

    for (size_t i = 0; i < root->children.size(); ++i)
    {
        bool isLast = (i == root->children.size() - 1);
        printTreeHelper(root->children[i], "", isLast, out);
    }
}