#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <climits>

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
    
    string getString() const { return string(str); }
    
    bool operator<(const Key& other) const {
        int cmp = strcmp(str, other.str);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }
    
    bool operator==(const Key& other) const {
        return strcmp(str, other.str) == 0 && value == other.value;
    }
    
    bool operator<=(const Key& other) const {
        return !(other < *this);
    }
    
    bool operator>(const Key& other) const {
        return other < *this;
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
    fstream dataFile;
    int rootPos;
    
    void clearAll() {
        dataFile.close();
        dataFile.open(DATA_FILE, ios::out | ios::binary | ios::trunc);
        dataFile.close();
        dataFile.open(DATA_FILE, ios::in | ios::out | ios::binary);
        
        rootPos = -1;
        dataFile.write(reinterpret_cast<char*>(&rootPos), sizeof(int));
        dataFile.flush();
    }
    
    void load() {
        dataFile.seekg(0, ios::beg);
        dataFile.read(reinterpret_cast<char*>(&rootPos), sizeof(int));
    }
    
    void saveRoot() {
        dataFile.seekp(0, ios::beg);
        dataFile.write(reinterpret_cast<char*>(&rootPos), sizeof(int));
        dataFile.flush();
    }
    
    int allocNode() {
        dataFile.seekp(0, ios::end);
        int pos = dataFile.tellp();
        BPTNode node;
        dataFile.write(reinterpret_cast<char*>(&node), sizeof(BPTNode));
        dataFile.flush();
        return pos;
    }
    
    void writeNodeS(int pos, const BPTNode& node) {
        dataFile.seekp(pos, ios::beg);
        dataFile.write(reinterpret_cast<const char*>(&node), sizeof(BPTNode));
        dataFile.flush();
    }
    
    BPTNode readNodeS(int pos) {
        BPTNode node;
        dataFile.seekg(pos, ios::beg);
        dataFile.read(reinterpret_cast<char*>(&node), sizeof(BPTNode));
        return node;
    }
    
public:
    BPTree() {
        dataFile.open(DATA_FILE, ios::in | ios::binary);
        if (!dataFile.is_open()) {
            clearAll();
        } else {
            dataFile.close();
            dataFile.open(DATA_FILE, ios::in | ios::out | ios::binary);
            load();
        }
    }
    
    ~BPTree() {
        if (dataFile.is_open()) dataFile.close();
    }
    
    void insert(const string& index, int value) {
        Key key(index, value);
        
        if (rootPos == -1) {
            int pos = allocNode();
            BPTNode node;
            node.isLeaf = true;
            node.keys[0] = key;
            node.numKeys = 1;
            writeNodeS(pos, node);
            rootPos = pos;
            saveRoot();
            return;
        }
        
        vector<int> path;
        vector<int> childIdx;
        int current = rootPos;
        BPTNode node = readNodeS(current);
        
        while (!node.isLeaf) {
            path.push_back(current);
            int i = 0;
            while (i < node.numKeys && strcmp(node.keys[i].str, index.c_str()) <= 0) {
                i++;
            }
            childIdx.push_back(i);
            current = node.children[i];
            node = readNodeS(current);
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
            writeNodeS(current, node);
            return;
        }
        
        int mid = node.numKeys / 2;
        int newLeaf = allocNode();
        BPTNode rightNode;
        rightNode.isLeaf = true;
        
        for (int j = mid; j < node.numKeys; j++) {
            rightNode.keys[j - mid] = node.keys[j];
        }
        rightNode.numKeys = node.numKeys - mid;
        rightNode.next = node.next;
        node.numKeys = mid;
        node.next = newLeaf;
        
        writeNodeS(current, node);
        writeNodeS(newLeaf, rightNode);
        
        Key splitKey = rightNode.keys[0];
        splitUp(path, childIdx, splitKey, current, newLeaf);
    }
    
    void splitUp(vector<int>& path, vector<int>& childIdx, Key splitKey, int leftChild, int rightChild) {
        while (!path.empty()) {
            int parent = path.back();
            path.pop_back();
            int idx = childIdx.back();
            childIdx.pop_back();
            
            BPTNode pNode = readNodeS(parent);
            
            for (int j = pNode.numKeys; j > idx; j--) {
                pNode.keys[j] = pNode.keys[j - 1];
                pNode.children[j + 1] = pNode.children[j];
            }
            pNode.keys[idx] = splitKey;
            pNode.children[idx] = leftChild;
            pNode.children[idx + 1] = rightChild;
            pNode.numKeys++;
            
            if (pNode.numKeys < ORDER) {
                writeNodeS(parent, pNode);
                return;
            }
            
            int mid = pNode.numKeys / 2;
            int newNode = allocNode();
            BPTNode internal;
            internal.isLeaf = false;
            
            splitKey = pNode.keys[mid];
            
            for (int j = mid + 1; j < pNode.numKeys; j++) {
                internal.keys[j - mid - 1] = pNode.keys[j];
                internal.children[j - mid - 1] = pNode.children[j];
            }
            internal.children[pNode.numKeys - mid - 1] = pNode.children[pNode.numKeys];
            internal.numKeys = pNode.numKeys - mid - 1;
            pNode.numKeys = mid;
            
            writeNodeS(parent, pNode);
            writeNodeS(newNode, internal);
            
            leftChild = parent;
            rightChild = newNode;
        }
        
        int newRoot = allocNode();
        BPTNode root;
        root.isLeaf = false;
        root.keys[0] = splitKey;
        root.children[0] = leftChild;
        root.children[1] = rightChild;
        root.numKeys = 1;
        writeNodeS(newRoot, root);
        rootPos = newRoot;
        saveRoot();
    }
    
    void deleteKey(const string& index, int value) {
        if (rootPos == -1) return;
        
        int current = rootPos;
        BPTNode node = readNodeS(current);
        
        while (!node.isLeaf) {
            int i = 0;
            while (i < node.numKeys && strcmp(node.keys[i].str, index.c_str()) <= 0) {
                i++;
            }
            current = node.children[i];
            node = readNodeS(current);
        }
        
        for (int i = 0; i < node.numKeys; i++) {
            if (strcmp(node.keys[i].str, index.c_str()) == 0 && node.keys[i].value == value) {
                for (int j = i; j < node.numKeys - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                }
                node.numKeys--;
                writeNodeS(current, node);
                return;
            }
        }
    }
    
    vector<int> find(const string& index) {
        vector<int> result;
        if (rootPos == -1) return result;
        
        int current = rootPos;
        BPTNode node = readNodeS(current);
        
        while (!node.isLeaf) {
            int i = 0;
            while (i < node.numKeys && strcmp(node.keys[i].str, index.c_str()) <= 0) {
                i++;
            }
            current = node.children[i];
            node = readNodeS(current);
        }
        
        while (current != -1) {
            for (int i = 0; i < node.numKeys; i++) {
                int cmp = strcmp(node.keys[i].str, index.c_str());
                if (cmp == 0) {
                    result.push_back(node.keys[i].value);
                } else if (cmp > 0) {
                    return result;
                }
            }
            current = node.next;
            if (current != -1) {
                node = readNodeS(current);
            }
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
