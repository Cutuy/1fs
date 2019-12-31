# 1fs
A file system projection provider

## Problem Statement
File clutters & heterogeneous files created in collaborative work result in inefficient knowledge finding and obscures knowledge creation

## Design Principles
  - Minimized up-front cost and "runtime" overhead for the user
  - Maximized freedom of choice in terms of of cloud drives, directory explorer, note-taking software, etc.

## Objectives
* Decluttered record keeping
  - File and directory as unit of knowledge
  - Basic versioning, plus support for editable (DOCX) vs exported (PDF) versions
  - Allow virtual files projected from applications
  - Allow files projected from multiple sources (local fs, cloud, web url)
* Effective knowledge finding
  - Allow personal organization of knowledge in addition to the public view
  - Enable "structured" file search (rather than keyword), that is search by path, by version, and by editable/exported type
  - Allow directory browsing by dir, ls, File Explorer, etc.
* Unlimited knowledge creation (Future)
  - Integration with RCS (git)
  - Sync back changed from personal view to public view
  
## Technical ideas
- stack of files (group of files from all formats/versions)
- virtual file API
- CX CI (continuous export continuous integration)
- allow each person maintain a directory structure at his/her will
- new changes in public merge to personal: automatic projection, pending review
- multiple "views" of a directry

## Examples
- mixed use of OneNote and txt for meeting notes
- "where's our latest business report"
- team unfamiliar with other's work documented everywhere in multiple format
- mixture of pdfs and docx
- file naming pain
- deep folders
- "scratch folder" (folder for code, CAD, poster design, etc.) whether to sync or not
- reproduction-preserving, or just knowledge-presenting-preserving level
- open notepad to record something
