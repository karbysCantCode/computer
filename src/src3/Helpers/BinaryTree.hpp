#ifndef BINARYTREE_H
#define BINARYTREE_H

#include <stack>
#include <vector>

template<typename T, typename Key, typename Compare>
class BinarySearchTree {
  struct Node {
    T* value;

    Node* left = nullptr;
    Node* right = nullptr;

    Node(T* value)
      : value(value) {}
  };

  Node* m_root = nullptr;
  Compare m_compare;

  void deleteNodes(Node* node) {
    if (!node)
      return;

    deleteNodes(node->left);
    deleteNodes(node->right);

    delete node;
  }

public:
  class Iterator {
    std::stack<Node*> m_stack;
    BinarySearchTree* m_tree;

    void pushLeft(Node* node) {
      while (node) {
        m_stack.push(node);
        node = node->left;
      }
    }

  public:
    std::stack<Node*>& getStack() {return m_stack;}
    Iterator(BinarySearchTree* tree, Node* root)
      : m_tree(tree) {
      pushLeft(root);
    }

    size_t getRemainingNodesInIterator() const {
      std::stack<Node*> stack = m_stack;
      size_t count = 0;

      while (!stack.empty()) {
        Node* node = stack.top();
        stack.pop();
        ++count;

        Node* current = node->right;

        while (current) {
          stack.push(current);
          current = current->left;
        }
      }

      return count;
    }
    
    Iterator(BinarySearchTree* tree, Node* node, Node* root)
      : m_tree(tree) {
      Node* current = root;

      while (current != nullptr) {
        if (current == node) {
          m_stack.push(current);
          return;
        }

        if (m_tree->m_compare(node->value, current->value)) {
          // going left: current is a legitimate future successor, keep it
          m_stack.push(current);
          current = current->left;
        } else {
          // going right: current is already "done", don't keep it
          current = current->right;
        }
      }

      // node wasn't actually found
      while (!m_stack.empty())
        m_stack.pop();
    }

    T& operator*() {
      return *m_stack.top()->value;
    }

    T* operator->() {
      return m_stack.top()->value;
    }

    Iterator& operator++() {
      Node* node = m_stack.top();
      m_stack.pop();

      if (node->right) {
        pushLeft(node->right);
      }

      return *this;
    }

    Iterator& begin() {
      return *this;
    }

    Iterator end() {
      return Iterator(m_tree, nullptr);
    }

    bool operator==(const Iterator& other) const {
      if (m_stack.empty() && other.m_stack.empty())
        return true;

      if (m_stack.empty() || other.m_stack.empty())
        return false;

      return m_stack.top() == other.m_stack.top();
    }

    bool operator!=(const Iterator& other) const {
      return !(*this == other);
    }

  };

  ~BinarySearchTree() {
    deleteNodes(m_root);
  }

  Iterator begin() {
    return Iterator(this, m_root);
  }

  Iterator end() {
    return Iterator(this, nullptr);
  }

  Iterator find(const Key& key) {
  Node* current = m_root;

  while (current) {
    if (m_compare(key, current->value)) {
      current = current->left;
    } else if (m_compare(current->value, key)) {
      current = current->right;
    } else {
      return Iterator(this, current, m_root);
    }
  }

  return end();
}

  Iterator last() {
    Node* current = m_root;

    if (!current)
      return end();

    while (current->right)
      current = current->right;

    return Iterator(this, current, m_root);
  }

  void insert(T* value) {
    Node** current = &m_root;

    while (*current) {
      if (m_compare(value, (*current)->value)) {
        current = &(*current)->left;
      } else {
        current = &(*current)->right;
      }
    }

    *current = new Node(value);
  }

  void collectNodes(Node* node, std::vector<Node*>& nodes) {
    if (!node)
      return;

    collectNodes(node->left, nodes);
    nodes.push_back(node);
    collectNodes(node->right, nodes);
  }

  Node* buildBalanced(std::vector<Node*>& nodes, size_t begin, size_t end) {
    if (begin >= end)
      return nullptr;

    size_t middle = begin + (end - begin) / 2;

    Node* node = nodes[middle];

    node->left = buildBalanced(nodes, begin, middle);
    node->right = buildBalanced(nodes, middle + 1, end);

    return node;
  }

  void rebalance() {
    std::vector<Node*> nodes;
    collectNodes(m_root, nodes);

    m_root = buildBalanced(nodes, 0, nodes.size());
  }

  
};

#endif