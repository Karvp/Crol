#pragma once
#include <vector>
#include "nodes.h"

using namespace std;

class AST {
public:
	AST() {}

	void startScan(Node* root) {
		this->nodes.clear();
		this->scan(this->root = root);
	}

	void freeNodes() {
		for (auto& x : this->nodes) {
			delete x;
		}
	}

	const vector<Node*>& getNodes() const noexcept {
		return this->nodes;
	}

private:
	Node* root = NULL;
	vector<Node*> nodes;

	template <typename... Args>
	void scan(Node* node, Args... args) {
		this->scan(node);
		this->scan(args...);
	}

	void scan(Node* node);
};