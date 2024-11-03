# Envelope

A lightweight, modern Markdown note-taking application built with GTK and WebKit with less than 0.1s start time.

> ⚠️ **Note**: This project is currently in active development. Features and interfaces may change significantly.

## Features

- 📝 Full Markdown support with WYSIWYG editor
- 🌳 Hierarchical file organization with vault system
- 🌙 Dark mode support
- 💾 Autosave functionality
- 🔄 File versioning and backup
- 🎨 Modern, clean interface
- 🔍 Quick search and navigation
- 🖥️ Cross-platform (Linux)

## Current Status

This application is under active development. While core features are functional, you may encounter bugs or incomplete features. Contributions and feedback are welcome!

### Working Features
- Markdown editing with preview
- File system navigation
- Dark mode toggle
- Autosave functionality
- Basic file operations (create, rename, delete)

## Installation

### Dependencies

Before building Envelope, ensure you have the following dependencies installed:

```bash
# Ubuntu/Debian
sudo apt install gcc make gtk+-3.0-dev webkit2gtk-4.0-dev libcmark-dev

# Fedora
sudo dnf install gcc make gtk3-devel webkit2gtk3-devel libcmark-devel

# Arch Linux
sudo pacman -S gcc make gtk3 webkit2gtk cmark
```

1. Launch app
2. Choose a vault directory to store your notes
3. Start creating and organizing your notes!

### Key Features

- **Vault System**: Organize your notes in a dedicated directory
- **WYSIWYG Editor**: Rich text editing with Markdown preview
- **File Management**: 
  - Create, rename, and delete notes
  - Right-click context menu for file operations
- **Customization**:
  - Toggle dark mode
  - Enable/disable autosave
  - Customize save intervals

## Configuration

Envelope stores its configuration in `~/.config/notes-gui/user.conf`. You can manually edit this file or use the in-app settings.

## Contributing

Contributions are welcome! As this project is in its early stages, there are many opportunities to contribute. Please feel free to:

- Report bugs
- Suggest features
- Submit pull requests
- Improve documentation

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [Toast UI Editor](https://ui.toast.com/tui-editor) for the Markdown editor
- [GTK](https://www.gtk.org/) for the GUI framework
- [WebKit](https://webkit.org/) for the web rendering engine
- [cmark](https://github.com/commonmark/cmark) for Markdown parsing

## Contact

Your Name - [@yourusername](https://twitter.com/yourusername)

Project Link: [https://github.com/yourusername/envelope](https://github.com/yourusername/envelope)
