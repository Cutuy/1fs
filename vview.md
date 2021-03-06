Effective knowledge finding > Personal organization vs public view of directory

## Objective
[vview-o1 ✔] Provide the same view for an existing directory at another place

[vview-o2 🟢] Provide a different view for the projected directory

## Key Results
[vview-o1-kr1 ✔] Project a hierarchical directory with arbitrary in-mem data that supports placeholder enumeration

[vview-o1-kr2 ✔] An exisiting directory is "captured" by 1fs and then projected

[vview-o1-kr3 ✔] The file data, upon requested, is "projected" from its original location

<del>[vview-o2-kr1] Track moves of each individual file</del>

[vview-o2-kr1a ✔] Track moves of file for enumeration of directory

[vview-o2-kr1b ✔] Track moves of file for finding physical path of a placeholder

<del>[vview-o2-kr2 🟢] The tracking info is persisted then restored (disk<->mem)</del>

[vview-o2-kr2a ✔] Correctness of return values of in-mem projections for directories (ignoring ghost cache issues)

[vview-o2-kr2b ✔] Eliminate ghost cache of directory

[vview-o2-kr2c 🟢] Command-line & WinShell file renames to reflect on fs

[vview-o2-kr3] Restore the modified projection upon a new projection for all files previously projected

## Next (12/28)
Refactor required to correctly address diff between "file path" from projfs and win32
Code quality update
(later) Encapsulate in a class/lib that runs as a console app

## Notes (12/30)
It seems ProjFS does not support rename/move of a directory. This, however, is crtical to our feature...
- Workaround: create a shadow file for each directory, with which the user can move around


## Notes (12/31)
Needs to break down abit for [vview-o2-kr1] because it is f* too hard!

## Notes (1/1)
Yet it seems I finished [vview-o2-kr1] :)

## Notes (1/2)
- Always lazy load/find the physical path upon request; never do the opposite
- Changing [vview-o2-kr2 🟢] for clarity and reflect design changes
- lpathcmpW needs to handle special case of ("", "abc")... this means root vs root/abc
- Never use LPCWSTR (char ptr) as item type for stl containers; otherwise push_back will not copy the value
- Never erase while in loop of enumeration
- Okay to use stl's default comparator for (std::wstring, wchar_t[])
- [vview-o2-kr2a] almost done... enum done (bug not-free), placeholder pending

## Design principles (1/2)
- Lazy load the actual FS on disk as lazy as possible

## Known Issues (1/3) => TODO
- After vview, should investigate how to "sync back" changes; bypass dirty state or allow it?
- Looks like file moves must be handled with explicit PrjFileDelete, otherwise the fs still caches the old placeholder... (ghost cache) Checked in code & it should be fine
- [Bug fixed] Self cancelling remaps (a->b, then b->a) seems to make 1fs not responding to the newer change

## Next (1/3)
Math proof of correctness?

## Notes (1/3)
- Tired day, mostly hunting for bugs
- The "ghost cache" is still there with different GUID!!

## Next (1/5)
- _MOVE file use its folder's metadata for datetime

# Summary (1/6)
Finally found out basing 1fs on top of Windows ProjFS is inappropriate... Due to ProjFS's "merge" of local fs & projected fs, and more importantly,
the cache is not purged eagerly (except the the file state become Tombstone or simply be deleted explicitly), and that no API is available to 
*update* the file status directly, it results in the user view (either by dir or File Explorer) showing deleted (from src) files still.

For dst renamed/removed files, ProjFs handles correctly; but has no support for directories.
For src files, changes on them result in changes on result of dir enum & placeholder enum - but ProjFS still "remembers" and merges new results with older ones
except explicit deletion. However, 1fs does not know how src is changed.

We have two options now - try implementing src watcher (which greatly compromises the promise of lazy load), or try re-basing 1fs on file system filter.