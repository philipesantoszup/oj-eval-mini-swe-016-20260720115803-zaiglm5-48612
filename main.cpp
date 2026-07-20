#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sstream>
#include <climits>

using namespace std;

const int ORDER = 200; // B+ tree order (max keys per node)
const string DATA_FILE = "bpt_data.bin";

struct Key {
    char str[72];
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
    
    bool operator>=(const Key& other) const {
        return !(*this < other);
    }
};

struct BPTNode {
    bool isLeaf;
    int numKeys;
    Key keys[ORDER];
    int children[ORDER + 1];
    int next;
    int prev;
    
    BPTNode() : isLeaf(true), numKeys(0), next(-1), prev(-1) {
        memset(children, -1, sizeof(children));
    }
};

const int HEADER_SIZE = sizeof(int);

class BPTree {
private:
    fstream file;
    int rootPos;
    
    int allocateNode() {
        file.seekp(0, ios::end);
        int pos = file.tellp();
        BPTNode node;
        file.write(reinterpret_cast<char*>(&node), sizeof(BPTNode));
        file.flush();
        return pos;
    }
    
    void writeNode(int pos, BPTNode* node) {
        file.seekp(pos, ios::beg);
        file.write(reinterpret_cast<char*>(node), sizeof(BPTNode));
        file.flush();
    }
    
    void readNode(int pos, BPTNode* node) {
        file.seekg(pos, ios::beg);
        file.read(reinterpret_cast<char*>(node), sizeof(BPTNode));
    }
    
    void writeRoot() {
        file.seekp(0, ios::beg);
        file.write(reinterpret_cast<char*>(&rootPos), sizeof(int));
        file.flush();
    }
    
    void initFile() {
        file.open(DATA_FILE, ios::in | ios::out | ios::binary);
        if (!file.is_open()) {
            file.open(DATA_FILE, ios::out | ios::binary);
            rootPos = -1;
            file.write(reinterpret_cast<char*>(&rootPos), sizeof(int));
            file.close();
            file.open(DATA_FILE, ios::in | ios::out | ios::binary);
        }
        file.read(reinterpret_cast<char*>(&rootPos), sizeof(int));
    }
    
    int findLeaf(const string& index) {
        if (rootPos == -1) return -1;
        
        int current = rootPos;
        BPTNode node;
        readNode(current, &node);
        
        while (!node.isLeaf) {
            Key searchKey(index, INT_MIN);
            int i = 0;
            while (i < node.numKeys && node.keys[i] <= searchKey) {
                i++;
            }
            if (i >= node.numKeys) {
                current = node.children[node.numKeys];
            } else {
                current = node.children[i];
            }
            readNode(current, &node);
        }
        return current;
    }
    
public:
    BPTree() { initFile(); }
    ~BPTree() { if (file.is_open()) file.close(); }
    
    void insert(const string& index, int value) {
        Key key(index, value);
        
        if (rootPos == -1) {
            int newPos = allocateNode();
            BPTNode node;
            node.isLeaf = true;
            node.keys[0] = key;
            node.numKeys = 1;
            writeNode(newPos, &node);
            rootPos = newPos;
            writeRoot();
            return;
        }
        
        vector<int> path;
        vector<int> indices;
        int current = rootPos;
        BPTNode node;
        readNode(current, &node);
        
        Key searchKey(index, INT_MIN);
        while (!node.isLeaf) {
            path.push_back(current);
            int i = 0;
            while (i < node.numKeys && node.keys[i] <= searchKey) {
                i++;
            }
            indices.push_back(i);
            current = node.children[i];
            readNode(current, &node);
        }
        
        // Check duplicate
        for (int i = 0; i < node.numKeys; i++) {
            if (strcmp(node.keys[i].str, index.c_str()) == 0 && node.keys[i].value == value) {
                return;
            }
        }
        
        // Insert into leaf
        int i = node.numKeys - 1;
        while (i >= 0 && key < node.keys[i]) {
            node.keys[i + 1] = node.keys[i];
            i--;
        }
        node.keys[i + 1] = key;
        node.numKeys++;
        
        if (node.numKeys < ORDER) {
            writeNode(current, &node);
        } else {
            // Split
            int mid = node.numKeys / 2;
            int newLeafPos = allocateNode();
            BPTNode newLeaf;
            newLeaf.isLeaf = true;
            
            for (int j = mid; j < node.numKeys; j++) {
                newLeaf.keys[j - mid] = node.keys[j];
            }
            newLeaf.numKeys = node.numKeys - mid;
            node.numKeys = mid;
            
            newLeaf.next = node.next;
            newLeaf.prev = current;
            node.next = newLeafPos;
            
            writeNode(current, &node);
            writeNode(newLeafPos, &newLeaf);
            
            Key splitKey = newLeaf.keys[0];
            
            if (path.empty()) {
                // New root
                int newRootPos = allocateNode();
                BPTNode newRoot;
                newRoot.isLeaf = false;
                newRoot.keys[0] = splitKey;
                newRoot.children[0] = current;
                newRoot.children[1] = newLeafPos;
                newRoot.numKeys = 1;
                writeNode(newRootPos, &newRoot);
                rootPos = newRootPos;
                writeRoot();
            } else {
                // Insert into parent
                while (!path.empty()) {
                    int parentPos = path.back();
                    path.pop_back();
                    int idx = indices.back();
                    indices.pop_back();
                    
                    BPTNode parent;
                    readNode(parentPos, &parent);
                    
                    // Shift keys and children
                    for (int j = parent.numKeys; j > idx; j--) {
                        parent.keys[j] = parent.keys[j - 1];
                        parent.children[j + 1] = parent.children[j];
                    }
                    parent.keys[idx] = splitKey;
                    parent.children[idx + 1] = newLeafPos;
                    parent.numKeys++;
                    
                    if (parent.numKeys < ORDER) {
                        writeNode(parentPos, &parent);
                        break;
                    } else {
                        // Split internal node
                        mid = parent.numKeys / 2;
                        int newInternalPos = allocateNode();
                        BPTNode newInternal;
                        newInternal.isLeaf = false;
                        
                        splitKey = parent.keys[mid];
                        
                        for (int j = mid + 1; j < parent.numKeys; j++) {
                            newInternal.keys[j - mid - 1] = parent.keys[j];
                            newInternal.children[j - mid - 1] = parent.children[j];
                        }
                        newInternal.children[parent.numKeys - mid - 1] = parent.children[parent.numKeys];
                        newInternal.numKeys = parent.numKeys - mid - 1;
                        parent.numKeys = mid;
                        
                        writeNode(parentPos, &parent);
                        writeNode(newInternalPos, &newInternal);
                        newLeafPos = newInternalPos;
                        
                        if (path.empty()) {
                            int newRootPos = allocateNode();
                            BPTNode newRoot;
                            newRoot.isLeaf = false;
                            newRoot.keys[0] = splitKey;
                            newRoot.children[0] = parentPos;
                            newRoot.children[1] = newInternalPos;
                            newRoot.numKeys = 1;
                            writeNode(newRootPos, &newRoot);
                            rootPos = newRootPos;
                            writeRoot();
                            break;
                        }
                    }
                }
            }
        }
    }
    
    void deleteKey(const string& index, int value) {
        if (rootPos == -1) return;
        
        int current = rootPos;
        BPTNode node;
        readNode(current, &node);
        
        // Find leaf
        Key searchKey(index, INT_MIN);
        while (!node.isLeaf) {
            int i = 0;
            while (i < node.numKeys && node.keys[i] <= searchKey) {
                i++;
            }
            current = node.children[i];
            readNode(current, &node);
        }
        
        // Find and delete
        for (int i = 0; i < node.numKeys; i++) {
            if (strcmp(node.keys[i].str, index.c_str()) == 0 && node.keys[i].value == value) {
                for (int j = i; j < node.numKeys - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                }
                node.numKeys--;
                writeNode(current, &node);
                return;
            }
        }
    }
    
    vector<int> find(const string& index) {
        vector<int> result;
        if (rootPos == -1) return result;
        
        int current = rootPos;
        BPTNode node;
        readNode(current, &node);
        
        Key searchKey(index, INT_MIN);
        while (!node.isLeaf) {
            int i = 0;
            while (i < node.numKeys && node.keys[i] <= searchKey) {
                i++;
            }
            current = node.children[i];
            readNode(current, &node);
        }
        
        // Traverse leaf nodes
        while (current != -1) {
            for (int i = 0; i < node.numKeys; i++) {
                string keyStr = node.keys[i].getString();
                if (keyStr == index) {
                    result.push_back(node.keys[i].value);
                } else if (keyStr > index) {
                    return result;
                }
            }
            current = node.next;
            if (current != -1) {
                readNode(current, &node);
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
