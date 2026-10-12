\# PA-02 Report — Re-engineering the `ls` Utility



\*\*Student:\*\* Atif Hussain

\*\*Roll No:\*\* BSDSF24M037

\*\*Course:\*\* Operating Systems

\*\*Assignment:\*\* PA-02 — ls Utility

\*\*Repository:\*\* https://github.com/ATIF-HUSSAIN100/BSDSF24M037-OS-A02



\---



\## Feature 2 (v1.1.0) — Complete Long Listing Format



\### Q1: What is the crucial difference between `stat()` and `lstat()`? When is `lstat()` more appropriate in `ls`?



\*\*`stat()`\*\* follows symbolic links — if the given path is a symlink, it returns metadata about the \*\*target file\*\* the link points to (its size, permissions, type, timestamp, etc.).



\*\*`lstat()`\*\* does \*\*not\*\* follow symbolic links — it returns metadata about the \*\*link itself\*\*. Its size is the length of the target path string, its type is `S\_IFLNK`, and its permissions are typically `lrwxrwxrwx`.



\*\*When to use `lstat()` in `ls`:\*\*

1\. \*\*Displaying symlinks as symlinks.\*\* Real `ls -l` shows `link -> target`. If we used `stat()`, a symlink to a directory would appear as `d` (directory) and we'd lose the `->` display entirely.

2\. \*\*Recursive listing (`-R`).\*\* We must not follow symlinks during recursion, otherwise a symlink pointing to a parent directory would cause infinite loops. `lstat()` lets us detect `S\_ISLNK` and skip such entries.

3\. \*\*Accurate type reporting.\*\* The user needs to know the entry \*is\* a link, not what it points to.



In short: use `stat()` when you care about the target's contents; use `lstat()` when you care about the entry as it exists in the directory.



\---



\### Q2: The `st\_mode` field contains both file type and permission bits. How do you extract them with bitwise operators and predefined macros?



`st\_mode` is a `mode\_t` integer bitmask. It has two logical parts:



\- \*\*File type:\*\* the high bits. Mask with `S\_IFMT` (`0xF000`) and compare against type constants.

\- \*\*Permissions:\*\* the low 12 bits (9 standard permission bits + 3 special bits: setuid, setgid, sticky).



\*\*Extracting file type:\*\*

```c

if ((st.st\_mode \& S\_IFMT) == S\_IFDIR) { /\* directory \*/ }

else if ((st.st\_mode \& S\_IFMT) == S\_IFLNK) { /\* symlink \*/ }

The POSIX macros are cleaner and equivalent:



c

if (S\_ISDIR(st.st\_mode))  { /\* directory \*/ }

if (S\_ISLNK(st.st\_mode))  { /\* symbolic link \*/ }

if (S\_ISREG(st.st\_mode))  { /\* regular file \*/ }

if (S\_ISCHR(st.st\_mode))  { /\* character device \*/ }

if (S\_ISBLK(st.st\_mode))  { /\* block device \*/ }

if (S\_ISFIFO(st.st\_mode)) { /\* named pipe \*/ }

if (S\_ISSOCK(st.st\_mode)) { /\* socket \*/ }

These are defined in <sys/stat.h> — each S\_ISxxx(m) expands to (((m) \& S\_IFMT) == S\_IFxxx).



Extracting permissions (each is an independent bit):



c

if (st.st\_mode \& S\_IRUSR)  /\* owner can read     (0400) \*/

if (st.st\_mode \& S\_IWUSR)  /\* owner can write    (0200) \*/

if (st.st\_mode \& S\_IXUSR)  /\* owner can execute  (0100) \*/

if (st.st\_mode \& S\_IRGRP)  /\* group can read     (0040) \*/

if (st.st\_mode \& S\_IWGRP)  /\* group can write    (0020) \*/

if (st.st\_mode \& S\_IXGRP)  /\* group can execute  (0010) \*/

if (st.st\_mode \& S\_IROTH)  /\* others can read    (0004) \*/

if (st.st\_mode \& S\_IWOTH)  /\* others can write   (0002) \*/

if (st.st\_mode \& S\_IXOTH)  /\* others can execute (0001) \*/

Special bits (setuid, setgid, sticky):



c

if (st.st\_mode \& S\_ISUID)  /\* setuid (04000) \*/

if (st.st\_mode \& S\_ISGID)  /\* setgid (02000) \*/

if (st.st\_mode \& S\_ISVTX)  /\* sticky (01000) \*/

Building the rwx string — for each of the 9 permission bits, print the letter or -:



c

char buf\[11];

buf\[0] = S\_ISDIR(st.st\_mode) ? 'd' : S\_ISLNK(st.st\_mode) ? 'l' : '-';

buf\[1] = (st.st\_mode \& S\_IRUSR) ? 'r' : '-';

buf\[2] = (st.st\_mode \& S\_IWUSR) ? 'w' : '-';

buf\[3] = (st.st\_mode \& S\_IXUSR) ? 'x' : '-';

/\* ... and so on for group and others ... \*/

For special bits, the execute letter changes case: s/S for setuid/setgid (lowercase if execute is also set), t/T for sticky.

