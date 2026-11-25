#include <chrono>
#include <ctime>
#include <thread>
#include "SmartPlaylistManager.h"

const int MAX_LISTS = 5;
const int DEMO_SECONDS_PER_MINUTE = 2;
const string GENRES[6] = {"Rock", "Pop", "Hip Hop", "Classical", "Jazz", "Metal"};

struct Action { bool added; int songId; int list; };

Song songs[MAX_SONGS];
int songCount = 0;
Playlist* lists[MAX_LISTS];
int listCount = 0, active = 0;
Stack<int, 10> history;
Stack<Action, 20> undoStack;
Queue<int, 30> upcoming;
BST titleTree;
Graph relations;
bool sortByPlays = false;


int readInt(const string& prompt) {
    int x;
    cout << prompt;
    while (!(cin >> x)) {
        if (cin.eof()) exit(0);
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "Please enter a number: ";
    }
    cin.ignore(1000, '\n');
    return x;
}

string readText(const string& prompt) {
    string s;
    do {
        cout << prompt;
        if (!getline(cin, s)) exit(0);
    } while (s.empty());
    for (size_t i = 0; i < s.size(); i++) if (s[i] == '|') s[i] = '/';
    return s;
}

void clearScreen() {
#ifdef _WIN32
    int ignored = system("cls");
#else
    int ignored = system("clear");
#endif
    (void)ignored;
}

void pause() {
    string s;
    cout << "\nPress Enter to continue...";
    if (!getline(cin, s)) exit(0);
}

void run(void (*feature)()) { feature(); pause(); }


int showMenu(const string& title, const string& itemsText, const string& zeroLabel = "Back") {
    clearScreen();
    cout << "----------------------------------------\n" << title
         << "\n----------------------------------------\n";
    int count = 0;
    string item;
    for (size_t i = 0; !itemsText.empty() && i <= itemsText.size(); i++) {
        if (i == itemsText.size() || itemsText[i] == '|') { cout << ++count << ". " << item << "\n"; item = ""; }
        else item += itemsText[i];
    }
    cout << "0. " << zeroLabel << "\n";
    while (true) {
        int c = readInt("Choice: ");
        if (c >= 0 && c <= count) { clearScreen(); return c; }
        cout << "  Choose a number from 0 to " << count << ".\n";
    }
}


bool validSong(int id) { return id >= 0 && id < songCount && songs[id].active; }

int askSongId() {
    int id = readInt("Song ID: ");
    if (!validSong(id)) { cout << "  No song with that ID.\n"; return -1; }
    return id;
}

Playlist* cur() { return lists[active]; }

int copyCatalog(Song out[]) {
    int n = 0;
    for (int i = 0; i < songCount; i++)
        if (songs[i].active) out[n++] = songs[i];
    return n;
}


int sortedCatalog(Song out[]) {
    int n = copyCatalog(out);
    if (sortByPlays) quickSort(out, 0, n - 1, BY_PLAYS);
    else mergeSort(out, n, BY_TITLE);
    return n;
}

int pickSong() {
    Song a[MAX_SONGS];
    int n = sortedCatalog(a);
    for (int i = 0; i < n; i++) printShort(a[i]);
    return askSongId();
}


void playWithTimer(int id) {
    Song& s = songs[id];
    Node* node = cur()->find(id);
    if (node) cur()->current = node;
    s.playCount++;
    history.pushForget(id);

    int demo = (s.seconds * DEMO_SECONDS_PER_MINUTE + 30) / 60;
    if (demo < 1) demo = 1;
    clearScreen();
    cout << "NOW PLAYING\n" << s.title << " - " << s.artist << "\nAlbum: " << s.album
         << "\nActual duration: " << mmss(s.seconds) << "   Demo duration: " << demo << " sec\n";
    for (int t = 0; t <= demo; t++) {
        int filled = t * 20 / demo;
        cout << "\r[" << string(filled, '#') << string(20 - filled, '.') << "] remaining "
             << demo - t << " sec  " << flush;
        if (t < demo) this_thread::sleep_for(chrono::seconds(1));
    }
    cout << "\n\nSONG FINISHED: \"" << s.title << "\" has finished.\n";
}


void songScreen(const string& heading, Song a[], int n, bool detailed) {
    while (true) {
        clearScreen();
        cout << "---------- " << heading << " ----------\n";
        if (n == 0) { cout << "  (no songs)\n"; pause(); return; }
        for (int i = 0; i < n; i++) detailed ? printSong(a[i]) : printShort(a[i]);
        int id = readInt("Song ID to play (-1 = back): ");
        if (id == -1) return;
        if (validSong(id)) playWithTimer(id); else cout << "  No song with that ID.\n";
        pause();
    }
}


int registerSong(string title, string artist, string album, string genre, int seconds) {
    int steps;
    if (songCount == MAX_SONGS) { cout << "  Catalog is full.\n"; return -1; }
    if (titleTree.search(title, steps) != -1) { cout << "  That title already exists.\n"; return -1; }
    int id = songCount++;
    songs[id] = {id, title, artist, album, genre, seconds, true, 0};
    titleTree.insert(title, id);
    for (int i = 0; i < id; i++)
        if (songs[i].active && related(songs[i], songs[id])) relations.addEdge(id, i);
    return id;
}

void addSong() {
    string title = readText("Title: "), artist = readText("Artist: "), album = readText("Album: ");
    int g = showMenu("GENRE", "Rock|Pop|Hip Hop|Classical|Jazz|Metal", "Cancel");
    if (g == 0) return;
    int seconds = readInt("Duration (seconds): ");
    if (seconds <= 0) { cout << "  Duration must be positive.\n"; return; }
    int id = registerSong(title, artist, album, GENRES[g - 1], seconds);
    if (id < 0) return;
    cur()->add(songs[id]);
    undoStack.pushForget({true, id, active});
    cout << "  Added with ID " << id << " (also added to playlist '" << cur()->name << "').\n";
}

void deleteSong() {
    int id = pickSong();
    if (id < 0) return;
    for (int i = 0; i < listCount; i++) lists[i]->remove(id);
    titleTree.remove(songs[id].title);
    relations.removeVertex(id);
    songs[id].active = false;
    cout << "  Deleted '" << songs[id].title << "'.\n";
}
// BST in-order traversal gives A to Z
void songsAtoZ() {
    int ids[MAX_SONGS];
    int n = titleTree.inorder(ids);
    cout << "SONGS A-Z (" << n << " songs)\n";
    for (int i = 0; i < n; i++) printShort(songs[ids[i]]);
}

void allSongsMenu() {
    int c;
    while ((c = showMenu("ALL SONGS", "Show all songs|Songs A-Z|Add new song|Delete song", "Back to home")) != 0) {
        if (c == 1) { Song a[MAX_SONGS]; int n = sortedCatalog(a); songScreen("ALL SONGS", a, n, true); }
        else if (c == 2) run(songsAtoZ);
        else if (c == 3) run(addSong);
        else run(deleteSong);
    }
}

//search
void searchSongs(int choice) {
    string text = readText("Search text: ");
    Song all[MAX_SONGS], byTitle[MAX_SONGS];
    int n = sortedCatalog(all), idx[MAX_SONGS], steps;

    if (choice == 1) {
        for (int i = 0; i < n; i++) byTitle[i] = all[i];
        mergeSort(byTitle, n, BY_TITLE);
        int i = binarySearch(byTitle, n, text, steps);
        if (i >= 0) { cout << "Found:\n"; printSong(byTitle[i]); return; }
    }

    int m = linearSearch(all, n, choice == 3 ? 3 : choice - 1, text, idx, steps);
    for (int i = 0; i < m; i++) printSong(all[idx[i]]);
    cout << m << " song(s) found.\n";
}

void searchMenu() {
    int c;
    while ((c = showMenu("SEARCH", "Search by title|Search by artist|Search by genre",
                         "Back to home")) != 0) {
        searchSongs(c);
        pause();
    }
}

void browse(const string& heading, int field) {
    while (true) {
        string names[MAX_SONGS], text;
        int count = 0;
        if (field == 3) for (int i = 0; i < 6; i++) names[count++] = GENRES[i];
        else
            for (int i = 0; i < songCount; i++) {
                bool seen = false;
                for (int j = 0; j < count; j++)
                    if (lower(names[j]) == lower(fieldOf(songs[i], field))) seen = true;
                if (songs[i].active && !seen) names[count++] = fieldOf(songs[i], field);
            }
        for (int i = 0; i < count; i++) text += (i ? "|" : "") + names[i];
        int c = showMenu(heading + "S", text, "Back");
        if (c == 0) return;
        Song all[MAX_SONGS], match[MAX_SONGS];
        int n = sortedCatalog(all), k = 0;
        for (int i = 0; i < n; i++)
            if (lower(fieldOf(all[i], field)) == lower(names[c - 1])) match[k++] = all[i];
        songScreen(heading + ": " + names[c - 1], match, k, false);
    }
}

void browseMenu() {
    int c;
    while ((c = showMenu("BROWSE", "Album|Artist|Genre", "Back to home")) != 0)
        browse(c == 1 ? "ALBUM" : c == 2 ? "ARTIST" : "GENRE", c == 1 ? 2 : c == 2 ? 1 : 3);
}

//playlists  [Linked List + Stack (undo)]
void showPlaylist() { cur()->show(); }
//Playlist Addsong
void addToPlaylist() {
    int id = pickSong();
    if (id < 0) return;
    if (!cur()->add(songs[id])) { cout << "  Already in this playlist.\n"; return; }
    undoStack.pushForget({true, id, active});
    cout << "  Added to '" << cur()->name << "'.\n";
}

void removeFromPlaylist() {
    cur()->show();
    int id = readInt("Song ID to remove: ");
    if (!cur()->remove(id)) { cout << "  That song is not in this playlist.\n"; return; }
    undoStack.pushForget({false, id, active});
    cout << "  Removed from '" << cur()->name << "'.\n";
}

void undoLast() {
    Action a;
    if (!undoStack.pop(a)) { cout << "  Nothing to undo.\n"; return; }
    Playlist* p = lists[a.list];
    if (a.added) { p->remove(a.songId); cout << "  Undone: song removed from '" << p->name << "'.\n"; }
    else if (songs[a.songId].active) { p->add(songs[a.songId]); cout << "  Undone: song added back to the end of '" << p->name << "'.\n"; }
    else cout << "  Cannot restore: the song was deleted from the catalog.\n";
}
//Next Song Played
void playNext() {
    Playlist* p = cur();
    if (!p->head) { cout << "  Playlist is empty.\n"; return; }
    Node* next = p->current ? p->current->next : p->head;
    if (!next) { cout << "  PLAYLIST FINISHED: there is no next song.\n"; return; }
    playWithTimer(next->song.id);
}
//Previous Song Played
void playPrevious() {
    Playlist* p = cur();
    if (!p->current) { cout << "  Nothing is playing yet.\n"; return; }
    if (!p->current->prev) { cout << "  This is the first song.\n"; return; }
    playWithTimer(p->current->prev->song.id);
}

void shufflePlaylist() {
    if (cur()->size < 2) { cout << "  Need at least 2 songs.\n"; return; }
    Song a[MAX_SONGS];
    int n = cur()->toArray(a);
    shuffleSongs(a, n);
    cur()->rebuild(a, n);
    cur()->show();
}

void showHistory() {
    cout << "Recently played (most recent first), " << history.size() << " songs\n";
    for (int d = 0; d < history.size(); d++) {
        int id = history.peek(d);
        cout << "  " << d + 1 << ". " << songs[id].title << " - " << songs[id].artist
             << (songs[id].active ? "" : " (deleted)") << "\n";
    }
}

void playlistMenu() {
    int c;
    while ((c = showMenu("PLAYLIST: " + cur()->name,
            "Show playlist|Add song|Remove song|Undo last change|Play / Next song|Previous song|Shuffle|Recently played")) != 0) {
        if (c == 1) run(showPlaylist);
        else if (c == 2) run(addToPlaylist);
        else if (c == 3) run(removeFromPlaylist);
        else if (c == 4) run(undoLast);
        else if (c == 5) run(playNext);
        else if (c == 6) run(playPrevious);
        else if (c == 7) run(shufflePlaylist);
        else run(showHistory);
    }
}

void createPlaylist() {
    if (listCount == MAX_LISTS) { cout << "  Maximum " << MAX_LISTS << " playlists.\n"; pause(); return; }
    lists[listCount++] = new Playlist(readText("Playlist name: "));
}

void playlistHome() {
    while (true) {
        string text;
        for (int i = 0; i < listCount; i++) text += lists[i]->name + " (" + to_string(lists[i]->size) + " songs)|";
        int c = showMenu("PLAYLISTS", text + "+ Create new playlist", "Back to home");
        if (c == 0) return;
        if (c == listCount + 1) createPlaylist();
        else { active = c - 1; playlistMenu(); }
    }
}

//play queue  [Queue]
void showQueue() {
    cout << "PLAY QUEUE (" << upcoming.size() << " songs)\n";
    if (upcoming.empty()) cout << "  (empty)\n";
    for (int i = 0; i < upcoming.size(); i++) {
        int id = upcoming.at(i);
        cout << "  " << i + 1 << ". " << songs[id].title << " - " << songs[id].artist
             << (songs[id].active ? "" : " (deleted)") << "\n";
    }
}

void addToQueue() {
    int id = pickSong();
    if (id >= 0) cout << (upcoming.enqueue(id) ? "  Queued.\n" : "  Queue is full.\n");
}

void playFromQueue() {
    int id;
    while (upcoming.dequeue(id))
        if (songs[id].active) { playWithTimer(id); return; }
    cout << "  The queue is empty.\n";
}

void queueMenu() {
    int c;
    while ((c = showMenu("PLAY QUEUE", "Add song to queue|Show queue|Play next queued song",
                         "Back to home")) != 0) {
        if (c == 1) run(addToQueue);
        else if (c == 2) run(showQueue);
        else run(playFromQueue);
    }
}

//discover  [BFS + DFS]
void relatedSongs() {
    int id = pickSong();
    if (id < 0) return;

    int order[MAX_SONGS], level[MAX_SONGS];
    int n = relations.bfs(id, order, level);

    if (n <= 1) {
        cout << "  No related songs.\n";
        return;
    }

    cout << "RELATED TO '" << songs[id].title << "'\n";

    int group = 0;
    for (int i = 1; i < n; i++) {
        int g = (level[order[i]] == 1) ? 1 : 2;
        if (g != group) {
            group = g;
            cout << (g == 1 ? "-- Closely related --\n" : "-- Also related --\n");
        }
        printShort(songs[order[i]]);
    }
}

void exploreNetwork() {       // DFS
    int id = pickSong();
    if (id < 0) return;
    int order[MAX_SONGS];
    int n = relations.dfs(id, order);
    cout << "EXPLORING FROM '" << songs[id].title << "' (" << n << " songs)\n";
    for (int i = 0; i < n; i++) cout << "  " << i + 1 << ". " << songs[order[i]].title << " - " << songs[order[i]].artist << "\n";
}

void discoverMenu() {
    int c;
    while ((c = showMenu("DISCOVER", "Related songs|Explore music network", "Back to home")) != 0) {
        if (c == 1) run(relatedSongs);
        else run(exploreNetwork);
    }
}

//sort setting
void sortMenu() {
    int c = showMenu(string("SORT SONGS (now: ") + (sortByPlays ? "most listened" : "alphabetical") + ")",
                     "Alphabetical|Most listened", "Back to home");
    if (c != 0) sortByPlays = (c == 2);
}


void loadSamples() {
    struct Sample { const char *title, *artist, *album, *genre; int seconds; };

    Sample samples[] = {
        {"Yellow", "Coldplay", "Parachutes", "Rock", 267},
        {"Fix You", "Coldplay", "X&Y", "Rock", 295},
        {"Viva la Vida", "Coldplay", "Viva la Vida", "Rock", 242},
        {"Bohemian Rhapsody", "Queen", "A Night at the Opera", "Rock", 354},
        {"Don't Stop Me Now", "Queen", "Jazz", "Rock", 209},
        {"Hotel California", "Eagles", "Hotel California", "Rock", 391},
        {"Imagine", "John Lennon", "Imagine", "Rock", 183},
        {"Beat It", "Michael Jackson", "Thriller", "Rock", 258},
        {"Billie Jean", "Michael Jackson", "Thriller", "Pop", 294},
        {"Shape of You", "Ed Sheeran", "Divide", "Pop", 234},
        {"Perfect", "Ed Sheeran", "Divide", "Pop", 263},
        {"Blinding Lights", "The Weeknd", "After Hours", "Pop", 200},
        {"Lose Yourself", "Eminem", "8 Mile", "Hip Hop", 326},
        {"Stan", "Eminem", "The Marshall Mathers LP", "Hip Hop", 404},
        {"Take Five", "Dave Brubeck", "Time Out", "Jazz", 324},
        {"So What", "Miles Davis", "Kind of Blue", "Jazz", 562},
        {"Moonlight Sonata", "Beethoven", "Piano Sonata No. 14", "Classical", 360},
        {"Fur Elise", "Beethoven", "Bagatelle No. 25", "Classical", 180},
        {"Enter Sandman", "Metallica", "Metallica", "Metal", 331},
        {"Master of Puppets", "Metallica", "Master of Puppets", "Metal", 515}};
    for (const Sample& s : samples) {
        int id = registerSong(s.title, s.artist, s.album, s.genre, s.seconds);
        if (id >= 0) lists[0]->add(songs[id]);
    }
}

int main() {
    srand((unsigned)time(0));
    lists[0] = new Playlist("Main");
    listCount = 1;
    loadSamples();

    int c;
    while ((c = showMenu("SMART PLAYLIST MANAGER - HOME",
            "All Songs|Search|Browse (album / artist / genre)|Playlist|Play queue|Discover|Sort songs", "Exit")) != 0) {
        if (c == 1) allSongsMenu();
        else if (c == 2) searchMenu();
        else if (c == 3) browseMenu();
        else if (c == 4) playlistHome();
        else if (c == 5) queueMenu();
        else if (c == 6) discoverMenu();
        else sortMenu();
    }
    cout << "Goodbye!\n";
    for (int i = 0; i < listCount; i++) delete lists[i];
    return 0;
}
