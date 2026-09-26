# Q-Db: A Custom Relational Database Engine

Q-Db is a lightweight, persistent relational database management system built entirely from scratch in C++. It features a custom SQL parser, a slotted-page storage engine, B+Tree indexing, Write-Ahead Logging (WAL) for crash recovery, and an interactive REPL interface.

---

## 🌟 Features

* **Custom SQL Parser:** Hand-rolled lexical tokenizer and parser for standard DDL and DML commands (`CREATE`, `DROP`, `INSERT`, `SELECT`, `UPDATE`, `DELETE`).
* **Slotted Page Storage:** Manages fixed-size 4KB pages on disk with intelligent row insertion, in-place updates, and tombstone deletion.
* **B+Tree & Hash Indexing:** Radically accelerates point lookups (`=`) and range queries (`>`, `>=`) dynamically, completely bypassing full table scans.
* **Write-Ahead Logging (WAL):** Ensures ACID-like durability. Operations are logged before modifying the main database file, allowing crash recovery on startup.
* **Smart Auto-Increment IDs:** Intelligently finds and fills gaps in ID sequences when rows are deleted, maximizing storage density.
* **ASCII Table Formatter:** Dynamically calculates column widths to render beautiful MySQL-style ASCII tables in the terminal.

---

## 🚀 Getting Started

### Prerequisites
* A C++17 compatible compiler (`g++` or `clang++`).
* CMake (3.10 or higher).
* Make.

### Building the Project
Clone the repository and build using CMake:
```bash
# Generate build files
cmake .

# Compile the engine
make
```

### Running Q-Db
Launch the REPL by running the generated binary:
```bash
./tiny_db
```
*Note: On its first run, Q-Db automatically seeds a `users` table with 100 rows so you can start querying immediately.*

---

## 🛠️ Quickstart Tutorial (Operations)

Q-Db operates via an interactive Read-Eval-Print Loop (REPL). Here is a quick workflow on how you can create tables, insert data, and query it.

**1. Create a Table**
```sql
q-db> CREATE TABLE staff (id INT, name CHAR, age INT)
```

**2. Insert Data**
You can insert data by explicitly defining the ID, or by letting Q-Db's gap-filling algorithm auto-generate an ID for you:
```sql
q-db> INSERT INTO staff VALUES (1, 'Alice', 25)
q-db> INSERT INTO staff VALUES ('Bob', 30)  -- Auto-assigns ID 2
```

**3. Query Data**
Q-Db supports operators: `=`, `!=`, `<`, `<=`, `>`, `>=`. Ranges and exact matches are automatically accelerated by B-Trees where applicable.
```sql
q-db> SELECT * FROM staff WHERE age >= 25
q-db> SELECT * FROM staff WHERE name != 'Alice'
```

**4. Update and Delete**
Modify existing records in-place, or delete them safely (which marks them as tombstones in the slotted page).
```sql
q-db> UPDATE staff SET age = 26 WHERE id = 1
q-db> DELETE FROM staff WHERE id = 2
```

**5. Drop Table**
When you are done, you can drop the table (which deletes its data and unlinks its `.btree` files).
```sql
q-db> DROP TABLE IF EXISTS staff
```

---

## 🚧 Limitations (The "Simple DB" Philosophy)

Q-Db is designed first and foremost as an **educational project** to understand how enterprise databases work under the hood. To keep the architecture (like binary serialization and slotted page mathematics) clean and understandable, a few intentional design choices were made:

* **Fixed Internal Schema:** While the `CREATE TABLE` syntax is supported, the underlying C++ `Row` struct is currently hard-coded to a specific tuple layout `(id INT, name CHAR, age INT)`. This guarantees a fixed `ROW_SIZE`, making slot directory offsets and free-space mathematics perfectly predictable.
* **Limited Data Types & Size:** There is no `VARCHAR` or `BLOB` support. To ensure strict memory layouts, strings are capped at 32 characters.
* **Single-Threaded:** There is no concurrency control mechanism (no latches, locks, or MVCC). It is designed for single-connection REPL usage.
* **No Joins:** The SQL executor currently routes queries to single tables. Relational `JOIN` operations are not yet implemented.

---

## 📊 Performance Benchmarks

Q-Db was benchmarked on a simulated dataset of **100,000 user rows** to measure the efficiency of its custom B+Tree implementation versus standard full table scans.

| Query Type | Query Condition | Selectivity | Full Table Scan | B+Tree Index | Speedup |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Range Query** | `WHERE age > 95` | ~4.0% | ~22.5 ms | **2.6 ms** | **~8.6x** |
| **Early-Exit Point Query** | `WHERE age = 37` | ~1.0% | ~22.5 ms | **1.7 ms** | **~13.2x** |

**Why is it faster?**
In a Full Table Scan, Q-Db must read every 4KB page from disk into memory, iterate through the slotted page headers, and evaluate the condition against every row. With the **B+Tree**, Q-Db traverses logarithmic internal nodes to find the exact starting leaf, and (in the case of the `maxKey` early exit algorithm) stops scanning the instant the requested upper bound is exceeded.

---

## 🧠 Low-Level Design (LLD)

Q-Db is structured into modular layers, mimicking the architecture of enterprise databases like PostgreSQL or SQLite.

### 1. Architecture Overview
1. **Client / REPL:** Accepts raw string input from the user.
2. **Tokenizer & Parser:** Converts strings into `Token` streams, then uses a recursive descent parsing strategy to build an Abstract Syntax Tree (AST) representing the query (`SelectStmt`, `InsertStmt`, etc.).
3. **Executor:** The query planner and execution engine. Routes commands to the storage and index engines, making decisions on whether to use B-Trees, Hash Indexes, or Fallback Scans.
4. **Catalog:** Manages metadata. Keeps track of dynamic table schemas and maps tables to their allocated `PageIDs`.
5. **Storage Engine (DiskManager & Table):** Handles the physical layout of bytes on disk, buffering, and Page I/O.
6. **WAL Manager:** Intercepts disk writes to maintain a durable transaction log.

### 2. Storage Engine & Slotted Pages
The database is stored in a single binary file (`data.bin`). Memory is divided into fixed **4KB (4096 bytes) Pages**. 
To avoid fragmentation, Q-Db uses a **Slotted Page Architecture**:
* **Page Header (8 Bytes):** Contains the `slotCount` (4 bytes) and `freeSpaceOffset` (4 bytes).
* **Data Payload:** Rows are serialized and grow from the *beginning* of the free space downward.
* **Slot Directory (Pointers):** Grows from the *end* of the page upward. Each entry is 4 bytes (2 for `rowOffset`, 2 for `rowLength`).
* **Deletions:** Q-Db uses a "Tombstone" approach. The `rowOffset` in the slot directory is overwritten with `UINT16_MAX`, gracefully freeing the slot so it can be overwritten without requiring an expensive memory shift.
* **Updates:** Execute strictly in-place when sizes match, minimizing file fragmentation.

### 3. Indexing
* **B+Tree (`.btree` files):** A disk-backed B+Tree of Order 50. Internal nodes store routing keys and child pointers, while leaf nodes form a linked list of `PageID` + `SlotID` pointers. 
  * Range queries (`>=`, `>`) execute by finding the starting leaf and sliding laterally. 
  * An optimized `maxKey` early-exit ensures equality or upper-bound queries terminate perfectly without checking extraneous leaves.
* **Hash Index:** Maintained in memory for lightning-fast equality (`=`) lookups.

### 4. Write-Ahead Logging (WAL) & Crash Recovery
To prevent data corruption during unexpected shutdowns or power loss:
1. Every `INSERT`, `UPDATE`, or `DELETE` creates a `WALRecord`.
2. The record contains the operation type, the target `PageID`/`SlotID`, and the raw binary before/after image of the `Row`.
3. The record is flushed to `data.bin.wal` *before* the actual Page is written by the DiskManager.
4. On boot, Q-Db reads the WAL file. If it finds operations that were logged but not successfully persisted to the main `data.bin` file, it **replays** the exact binary payloads to the affected slots to restore total consistency.

---

## 📁 File Structure

```text
Q-Db/
├── CMakeLists.txt         # Build configuration
├── README.md              # Project documentation
├── src/
│   ├── main.cpp           # REPL loop, application entry point, DB bootstrapping
│   │
│   ├── Tokenizer.h/cpp    # Lexical analysis (strings -> SQL Tokens)
│   ├── Parser.h/cpp       # Syntactic analysis (Tokens -> AST structs)
│   │
│   ├── Executor.h/cpp     # Core query execution engine (AST -> Storage operations)
│   │
│   ├── Catalog.h/cpp      # Metadata manager (Tables, Schemas, Page allocations)
│   ├── Table.h/cpp        # High-level table abstraction (CRUD routing)
│   │
│   ├── DiskManager.h/cpp  # Low-level disk I/O, File streaming, Page allocation
│   ├── Page.h             # Slotted page memory layout and inline binary manipulators
│   ├── Row.h              # Tuple definition structures
│   │
│   ├── BTree.h/cpp        # Disk-backed B+Tree implementation (Order 50)
│   ├── HashIndex.h/cpp    # In-memory fast Hash Map index
│   │
│   └── WAL.h/cpp          # Write-Ahead Log manager and recovery replayer
```