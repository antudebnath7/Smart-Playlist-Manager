#ifndef SMART_PLAYLIST_MANAGER_H
#define SMART_PLAYLIST_MANAGER_H

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

const int MAX_SONGS = 100;

//SONG
struct Song {
    int id;
    string title, artist, album, genre;
    int seconds;
    bool active;
    int playCount;
};

string lower(string s) {
    for (char& c : s) c = tolower((unsigned char)c);
    return s;
}


bool related(const Song& a, const Song& b) {
    return lower(a.artist) == lower(b.artist) || lower(a.genre) == lower(b.genre);
}

string mmss(int seconds) {
    return to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + to_string(seconds % 60);
}


const string& fieldOf(const Song& s, int field) {
    return field == 0 ? s.title : field == 1 ? s.artist : field == 2 ? s.album : s.genre;
}

void printShort(const Song& s) {
    cout << "  [" << s.id << "] " << s.title << " - " << s.artist << "\n";
}

void printSong(const Song& s) {
    printShort(s);
    cout << "      " << s.album << " | " << s.genre << " | " << mmss(s.seconds)
         << " | plays: " << s.playCount << "\n";
}

//LINKED LIST
struct Node {
    Song song;
    Node* prev = nullptr;
    Node* next = nullptr;
    Node(const Song& s) : song(s) {}
};

class Playlist {
public:
    string name;
    Node* head = nullptr;
    Node* tail = nullptr;
    Node* current = nullptr;
    int size = 0;

    Playlist(string n) : name(n) {}
    ~Playlist() { clear(); }

    void clear() {
        while (head) { Node* next = head->next; delete head; head = next; }
        tail = current = nullptr;
        size = 0;
    }

    Node* find(int id) {
        for (Node* n = head; n; n = n->next)
            if (n->song.id == id) return n;
        return nullptr;
    }

    bool add(const Song& s) {
        if (find(s.id)) return false;
        Node* n = new Node(s);
        if (!tail) head = tail = n;
        else { tail->next = n; n->prev = tail; tail = n; }
        size++;
        return true;
    }

    bool remove(int id) {                      // delete a node
        Node* n = find(id);
        if (!n) return false;
        if (n->prev) n->prev->next = n->next; else head = n->next;
        if (n->next) n->next->prev = n->prev; else tail = n->prev;
        if (current == n) current = n->prev;
        delete n;
        size--;
        return true;
    }
//Show Playlist
    void show() {
        cout << "\nPlaylist '" << name << "' (" << size << " songs)\n";
        if (!head) cout << "  (empty)\n";
        int i = 1;
        for (Node* n = head; n; n = n->next, i++)
            cout << "  " << i << ". [" << n->song.id << "] " << n->song.title << " - " << n->song.artist
                 << (n == current ? "   <-- now playing" : "") << "\n";
    }

    int toArray(Song a[]) {
        int n = 0;
        for (Node* p = head; p; p = p->next) a[n++] = p->song;
        return n;
    }

    void rebuild(Song a[], int n) {
        int playing = current ? current->song.id : -1;
        clear();
        for (int i = 0; i < n; i++) add(a[i]);
        current = find(playing);
    }
};

//STACK
template <typename T, int CAP>
class Stack {
    T data[CAP];
    int count = 0;
public:
    bool empty() const { return count == 0; }
    int size() const { return count; }
    bool push(const T& x) {
        if (count == CAP) return false;
        data[count++] = x;
        return true;
    }
    void pushForget(const T& x) {
        if (count == CAP) {
            for (int i = 1; i < count; i++) data[i - 1] = data[i];
            count--;
        }
        data[count++] = x;
    }
    bool pop(T& x) {
        if (count == 0) return false;
        x = data[--count];
        return true;
    }
    T peek(int depth) const { return data[count - 1 - depth]; }
};

// QUEUE
template <typename T, int CAP>
class Queue {
    T data[CAP];
    int front = 0;
    int count = 0;
public:
    bool empty() const { return count == 0; }
    int size() const { return count; }
    bool enqueue(const T& x) {
        if (count == CAP) return false;
        data[(front + count) % CAP] = x;
        count++;
        return true;
    }
    bool dequeue(T& x) {
        if (count == 0) return false;
        x = data[front];
        front = (front + 1) % CAP;
        count--;
        return true;
    }
    T at(int i) const { return data[(front + i) % CAP]; }
};

//TREE
struct TreeNode {
    string key;
    int id;
    TreeNode* left = nullptr;
    TreeNode* right = nullptr;
    TreeNode(string k, int i) : key(k), id(i) {}
};

class BST {
    TreeNode* root = nullptr;

    TreeNode* insertNode(TreeNode* n, const string& key, int id, bool& added) {
        if (!n) { added = true; return new TreeNode(key, id); }
        if (key < n->key) n->left = insertNode(n->left, key, id, added);
        else if (key > n->key) n->right = insertNode(n->right, key, id, added);
        return n;
    }

    TreeNode* removeNode(TreeNode* n, const string& key) {
        if (!n) return nullptr;
        if (key < n->key) n->left = removeNode(n->left, key);
        else if (key > n->key) n->right = removeNode(n->right, key);
        else if (!n->left || !n->right) {
            TreeNode* child = n->left ? n->left : n->right;
            delete n;
            return child;
        } else {
            TreeNode* s = n->right;
            while (s->left) s = s->left;
            n->key = s->key;
            n->id = s->id;
            n->right = removeNode(n->right, s->key);
        }
        return n;
    }
//Inorder traversal
    void inorder(TreeNode* n, int out[], int& count) {
        if (!n) return;
        inorder(n->left, out, count);
        out[count++] = n->id;
        inorder(n->right, out, count);
    }

    void destroy(TreeNode* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }

public:
    ~BST() { destroy(root); }

    bool insert(const string& title, int id) {
        bool added = false;
        root = insertNode(root, lower(title), id, added);
        return added;
    }

    int search(const string& title, int& steps) {
        string key = lower(title);
        TreeNode* n = root;
        steps = 0;
        while (n) {
            steps++;
            if (key == n->key) return n->id;
            n = (key < n->key) ? n->left : n->right;
        }
        return -1;
    }

    void remove(const string& title) { root = removeNode(root, lower(title)); }
    int inorder(int out[]) { int count = 0; inorder(root, out, count); return count; }
};

//GRAPH
class Graph {
    vector<int> adj[MAX_SONGS];

    void dfsVisit(int u, bool seen[], int order[], int& count) {
        seen[u] = true;
        order[count++] = u;
        for (int v : adj[u])
            if (!seen[v]) dfsVisit(v, seen, order, count);
    }

public:
    void addEdge(int a, int b) { adj[a].push_back(b); adj[b].push_back(a); }

    void removeVertex(int v) {
        for (int u : adj[v]) {
            vector<int>& list = adj[u];
            for (size_t i = 0; i < list.size(); i++)
                if (list[i] == v) { list.erase(list.begin() + i); break; }
        }
        adj[v].clear();
    }

    // BFS
    int bfs(int start, int order[], int level[]) {
        for (int i = 0; i < MAX_SONGS; i++) level[i] = -1;
        Queue<int, MAX_SONGS> q;
        int count = 0, u;
        level[start] = 0;
        q.enqueue(start);
        while (q.dequeue(u)) {
            order[count++] = u;
            for (int v : adj[u])
                if (level[v] == -1) {
                    level[v] = level[u] + 1;
                    q.enqueue(v);
                }
        }
        return count;
    }

    int dfs(int start, int order[]) {
        bool seen[MAX_SONGS] = {false};
        int count = 0;
        dfsVisit(start, seen, order, count);
        return count;
    }
};

//SEARCHING
int linearSearch(const Song a[], int n, int field, const string& text, int found[], int& steps) {
    int matches = 0;
    steps = 0;
    for (int i = 0; i < n; i++) {
        steps++;
        if (lower(fieldOf(a[i], field)).find(lower(text)) != string::npos) found[matches++] = i;
    }
    return matches;
}

// Binary search
int binarySearch(const Song a[], int n, const string& title, int& steps) {
    string key = lower(title);
    int low = 0, high = n - 1;
    steps = 0;
    while (low <= high) {
        int mid = (low + high) / 2;
        steps++;
        string midTitle = lower(a[mid].title);
        if (midTitle == key) return mid;
        if (midTitle < key) low = mid + 1; else high = mid - 1;
    }
    return -1;
}

//SORTING
enum SortBy { BY_TITLE, BY_PLAYS };

void swapSongs(Song& a, Song& b) { Song t = a; a = b; b = t; }

bool before(const Song& a, const Song& b, SortBy by) {   // should a come before b?
    if (by == BY_PLAYS && a.playCount != b.playCount) return a.playCount > b.playCount;  // most played first
    return lower(a.title) < lower(b.title);              // A to Z (also breaks ties)
}

// Merge sort
void mergeSortRange(Song a[], Song tmp[], int lo, int hi, SortBy by) {   // sorts a[lo..hi-1]
    if (hi - lo < 2) return;
    int mid = (lo + hi) / 2;
    mergeSortRange(a, tmp, lo, mid, by);
    mergeSortRange(a, tmp, mid, hi, by);
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) tmp[k++] = before(a[j], a[i], by) ? a[j++] : a[i++];
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi) tmp[k++] = a[j++];
    for (k = lo; k < hi; k++) a[k] = tmp[k];
}
void mergeSort(Song a[], int n, SortBy by) {
    Song tmp[MAX_SONGS];
    mergeSortRange(a, tmp, 0, n, by);
}

// Quick sort
void quickSort(Song a[], int lo, int hi, SortBy by) {
    if (lo >= hi) return;
    swapSongs(a[(lo + hi) / 2], a[hi]);
    int p = lo;
    for (int j = lo; j < hi; j++)
        if (before(a[j], a[hi], by)) swapSongs(a[p++], a[j]);
    swapSongs(a[p], a[hi]);
    quickSort(a, lo, p - 1, by);
    quickSort(a, p + 1, hi, by);
}

void shuffleSongs(Song a[], int n) {
    for (int i = n - 1; i > 0; i--) swapSongs(a[i], a[rand() % (i + 1)]);
}

#endif
