#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int ORDER = 200;
const string DATA_FILE = "bpt_data.bin";

struct Key {
    char str[80];
    int value;
    
    Key() : value(0) { memset(str, 0, sizeof(str)); }
    Key(const string& s, int v) : value(v) {
        memset(str, 0, sizeof(str));
        strncpy(str, s.c_str(), sizeof(str) - 1);
    }
    
    bool operator<(const Key& other) const {
        int cmp = strcmp(str, other.str);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }
    
    bool operator==(const Key& other) const {
        return strcmp(str, other.str) == 0 && value == other.value;
    }
};

struct BPTNode {
    bool isLeaf;
    int numKeys;
    Key keys[ORDER];
    int children[ORDER + 1];
    int next;
    
    BPTNode() : isLeaf(true), numKeys(0), next(-1) {
        for (int i = 0; i < ORDER + 1; i++) children[i] = -1;
    }
};

class BPTree {
private:
    FILE* dataFile;
    int rootPos;
    
    void loadOrInit() {
        dataFile = fopen(DATA_FILE.c_str(), "rb+");
        if (!dataFile) {
            dataFile = fopen(DATA_FILE.c_str(), "wb+");
            rootPos = -1;
            fwrite(&rootPos, sizeof(int), 1, dataFile);
        } else {
            fread(&rootPos, sizeof(int), 1, dataFile);
        }
    }
    
    void writeRoot() {
        fseek(dataFile, 0, SEEK_SET);
        fwrite(&rootPos, sizeof(int), 1, dataFile);
    }
    
    int allocNode() {
        fseek(dataFile, 0, SEEK_END);
        int pos = ftell(dataFile);
        BPTNode node;
        fwrite(&node, sizeof(BPTNode), 1, dataFile);
        return pos;
    }
    
    void writeNode(int pos, const BPTNode& node) {
        fseek(dataFile, pos, SEEK_SET);
        fwrite(&node, sizeof(BPTNode), 1, dataFile);
    }
    
    BPTNode readNode(int pos) {
        BPTNode node;
        fseek(dataFile, pos, SEEK_SET);
        fread(&node, sizeof(BPTNode), 1, dataFile);
        return node;
    }
    
    // Compare only string parts for navigation purposes
    int compareStrOnly(const Key& key, const string& index) {
        return strcmp(key.str, index.c_str());
    }
    
public:
    BPTree() { loadOrInit(); }
    ~BPTree() { if (dataFile) { fflush(dataFile); fclose(dataFile); } }
    
    void insert(const string& index, int value) {
        Key key(index, value);
        
        if (rootPos == -1) {
            int pos = allocNode();
            BPTNode node;
            node.isLeaf = true;
            node.keys[0] = key;
            node.numKeys = 1;
            writeNode(pos, node);
            rootPos = pos;
            writeRoot();
            return;
        }
        
        vector<int> path;
        vector<int> childIdx;
        int current = rootPos;
        BPTNode node = readNode(current);
        
        while (!node.isLeaf) {
            path.push_back(current);
            int i = 0;
            // Use < for string-only comparison
            while (i < node.numKeys && strcmp(node.keys[i].str, index.c_str()) < 0) i++;
            childIdx.push_back(i);
            current = node.children[i];
            node = readNode(current);
        }
        
        for (int i = 0; i < node.numKeys; i++) {
            if (strcmp(node.keys[i].str, index.c_str()) == 0 && node.keys[i].value == value) {
                return;
            }
        }
        
        int i = node.numKeys - 1;
        while (i >= 0 && key < node.keys[i]) {
            node.keys[i + 1] = node.keys[i];
            i--;
        }
        node.keys[i + 1] = key;
        node.numKeys++;
        
        if (node.numKeys < ORDER) {
            writeNode(current, node);
            return;
        }
        
        int mid = node.numKeys / 2;
        int newLeaf = allocNode();
        BPTNode right;
        right.isLeaf = true;
        
        for (int j = mid; j < node.numKeys; j++) {
            right.keys[j - mid] = node.keys[j];
        }
        right.numKeys = node.numKeys - mid;
        right.next = node.next;
        node.numKeys = mid;
        node.next = newLeaf;
        
        writeNode(current, node);
        writeNode(newLeaf, right);
        
        Key splitKey = right.keys[0];
        insertParent(path, childIdx, splitKey, current, newLeaf);
    }
    
    void insertParent(vector<int>& path, vector<int>& childIdx, Key splitKey, int leftChild, int rightChild) {
        if (path.empty()) {
            int newRoot = allocNode();
            BPTNode root;
            root.isLeaf = false;
            root.keys[0] = splitKey;
            root.children[0] = leftChild;
            root.children[1] = rightChild;
            root.numKeys = 1;
            writeNode(newRoot, root);
            rootPos = newRoot;
            writeRoot();
            return;
        }
        
        int parent = path.back();
        path.pop_back();
        int idx = childIdx.back();
        childIdx.pop_back();
        
        BPTNode pNode = readNode(parent);
        
        for (int j = pNode.numKeys; j > idx; j--) {
            pNode.keys[j] = pNode.keys[j - 1];
            pNode.children[j + 1] = pNode.children[j];
        }
        pNode.keys[idx] = splitKey;
        pNode.children[idx + 1] = rightChild;
        pNode.numKeys++;
        
        if (pNode.numKeys < ORDER) {
            writeNode(parent, pNode);
            return;
        }
        
        int mid = pNode.numKeys / 2;
        int newInternal = allocNode();
        BPTNode rightNode;
        rightNode.isLeaf = false;
        
        splitKey = pNode.keys[mid];
        
        for (int j = mid + 1; j < pNode.numKeys; j++) {
            rightNode.keys[j - mid - 1] = pNode.keys[j];
            rightNode.children[j - mid - 1] = pNode.children[j];
        }
        rightNode.children[pNode.numKeys - mid - 1] = pNode.children[pNode.numKeys];
        rightNode.numKeys = pNode.numKeys - mid - 1;
        pNode.numKeys = mid;
        
        writeNode(parent, pNode);
        writeNode(newInternal, rightNode);
        
        insertParent(path, childIdx, splitKey, parent, newInternal);
    }
    
    void deleteKey(const string& index, int value) {
        if (rootPos == -1) return;
        
        int current = rootPos;
        BPTNode node = readNode(current);
        
        while (!node.isLeaf) {
            int i = 0;
            while (i < node.numKeys && strcmp(node.keys[i].str, index.c_str()) < 0) i++;
            current = node.children[i];
            node = readNode(current);
        }
        
        for (int i = 0; i < node.numKeys; i++) {
            if (strcmp(node.keys[i].str, index.c_str()) == 0 && node.keys[i].value == value) {
                for (int j = i; j < node.numKeys - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                }
                node.numKeys--;
                writeNode(current, node);
                return;
            }
        }
    }
    
    vector<int> find(const string& index) {
        vector<int> result;
        if (rootPos == -1) return result;
        
        int current = rootPos;
        BPTNode node = readNode(current);
        
        while (!node.isLeaf) {
            int i = 0;
            while (i < node.numKeys && strcmp(node.keys[i].str, index.c_str()) < 0) i++;
            current = node.children[i];
            node = readNode(current);
        }
        
        while (current != -1) {
            for (int i = 0; i < node.numKeys; i++) {
                int cmp = strcmp(node.keys[i].str, index.c_str());
                if (cmp == 0) result.push_back(node.keys[i].value);
                else if (cmp > 0) return result;
            }
            current = node.next;
            if (current != -1) node = readNode(current);
        }
        return result;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    BPTree bpt;
    
    int n;
    cin >> n;
    
    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;
        
        if (cmd == "insert") {
            string index;
            int value;
            cin >> index >> value;
            bpt.insert(index, value);
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;
            bpt.deleteKey(index, value);
        } else if (cmd == "find") {
            string index;
            cin >> index;
            vector<int> values = bpt.find(index);
            if (values.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << values[j];
                }
                cout << "\n";
            }
        }
    }
    
    return 0;
}
