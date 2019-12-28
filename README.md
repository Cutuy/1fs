# 1fs
A file system projection provider

## Problem Statement
File clutters & heterogeneous files created in collaborative work result in inefficient knowledge finding and obscures knowledge creation

## Design Principles
  - Minimalism in up-front cost and "runtime" overhead ("before you create xxx, you must")
  - Freedom of knowledge organization, use of cloud drives, 

## Objectives
* Decluttered record keeping
  - Versioning, Editable (DOCX) vs exported (PDF) (completeness)
  - File as unit of knowledge, allowing virtual files projected from applications
  - Allow files from multi source (local fs, cloud, web url)
* Effective knowledge finding
  - Personal organization vs public view of directory
  - Gather files by topic, file stack, editable/exported type
  - Browse by hierachy as if browsing a normal directory
* Unlimited knowledge creation
  - Integration with RCS for submodules
  - Backward propagation (personal -> public)
  
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
