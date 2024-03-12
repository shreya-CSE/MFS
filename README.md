# MFS - Simple File System Manipulation

## Overview

MFS (Simple File System Manipulation) is a command-line utility designed for basic file system operations on a FAT32 file system image. The program allows users to interact with the file system, performing tasks such as opening and closing the file system image, navigating directories, retrieving files, and reading file content.

## Features

- Open and close a FAT32 file system image.
- Display information about the file system image.
- Change the current working directory within the file system.
- Retrieve files from the file system and save them locally.
- Display statistics (size, attributes, etc.) of a specified file.
- List the contents of the current directory.

## Usage

```bash
./mfs
```

The command above starts the MFS shell. From there, you can enter various commands to interact with the file system image.

## Commands

### `open <FILENAME>`

Opens the specified FAT32 file system image.

```bash
open example.img
```

### `close`

Closes the currently opened file system image.

```bash
close
```

### `info`

Displays information about the currently opened file system image.

```bash
info
```

### `cd <DIRECTORY>`

Changes the current working directory within the file system.

```bash
cd /path/to/directory
```

### `get <FILENAME>`

Retrieves a file from the file system and saves it locally.

```bash
get file.txt
```

### `ls`

Lists the contents of the current directory.

```bash
ls
```

### `read <FILENAME> <POSITION> <BYTES>`

Reads a specified number of bytes from the specified position in a file.

```bash
read file.txt 100 50
```

### `stat <FILENAME>`

Displays statistics about a specified file.

```bash
stat file.txt
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
