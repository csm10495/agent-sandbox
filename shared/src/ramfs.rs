//! Pure-logic hierarchical in-memory filesystem.
//!
//! Paths are forward-slash separated. Absolute paths start with `/`. The FS
//! knows about its own current working directory so callers can resolve
//! relative paths. Nodes are either files (Vec<u8> contents) or directories
//! (map from child name to node id). Node ids are stable once allocated.

use alloc::string::{String, ToString};
use alloc::vec::Vec;

pub type NodeId = u32;
pub const ROOT: NodeId = 0;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FsError {
    NotFound,
    NotADirectory,
    IsADirectory,
    AlreadyExists,
    InvalidPath,
    NameTooLong,
    NotEmpty,
}

pub type FsResult<T> = Result<T, FsError>;

#[derive(Debug)]
enum NodeKind {
    File(Vec<u8>),
    Dir(Vec<(String, NodeId)>),
}

#[derive(Debug)]
struct Node {
    kind: NodeKind,
    parent: NodeId,
}

#[derive(Debug)]
pub struct RamFs {
    nodes: Vec<Node>,
    cwd: NodeId,
}

const MAX_NAME: usize = 255;

impl Default for RamFs {
    fn default() -> Self {
        Self::new()
    }
}

impl RamFs {
    pub fn new() -> Self {
        let mut fs = Self { nodes: Vec::new(), cwd: ROOT };
        fs.nodes.push(Node {
            kind: NodeKind::Dir(Vec::new()),
            parent: ROOT, // root is its own parent
        });
        fs
    }

    pub fn cwd(&self) -> NodeId {
        self.cwd
    }

    /// Full path of `id` as a newly-allocated String.
    pub fn path_of(&self, id: NodeId) -> String {
        if id == ROOT {
            return "/".to_string();
        }
        let mut parts: Vec<&str> = Vec::new();
        let mut cur = id;
        while cur != ROOT {
            let parent = self.nodes[cur as usize].parent;
            let name = match &self.nodes[parent as usize].kind {
                NodeKind::Dir(children) => children
                    .iter()
                    .find(|(_, nid)| *nid == cur)
                    .map(|(n, _)| n.as_str())
                    .unwrap_or(""),
                _ => "",
            };
            parts.push(name);
            cur = parent;
        }
        let mut out = String::new();
        for p in parts.iter().rev() {
            out.push('/');
            out.push_str(p);
        }
        if out.is_empty() {
            out.push('/');
        }
        out
    }

    /// Resolve a path relative to `start`, or absolute.
    pub fn resolve_from(&self, start: NodeId, path: &str) -> FsResult<NodeId> {
        if path.is_empty() {
            return Err(FsError::InvalidPath);
        }
        let mut cur = if path.starts_with('/') { ROOT } else { start };
        for part in path.split('/') {
            if part.is_empty() || part == "." {
                continue;
            }
            if part == ".." {
                cur = self.nodes[cur as usize].parent;
                continue;
            }
            cur = self.child(cur, part)?;
        }
        Ok(cur)
    }

    pub fn resolve(&self, path: &str) -> FsResult<NodeId> {
        self.resolve_from(self.cwd, path)
    }

    fn child(&self, dir: NodeId, name: &str) -> FsResult<NodeId> {
        match &self.nodes[dir as usize].kind {
            NodeKind::Dir(children) => children
                .iter()
                .find(|(n, _)| n == name)
                .map(|(_, id)| *id)
                .ok_or(FsError::NotFound),
            NodeKind::File(_) => Err(FsError::NotADirectory),
        }
    }

    pub fn is_dir(&self, id: NodeId) -> bool {
        matches!(self.nodes[id as usize].kind, NodeKind::Dir(_))
    }

    pub fn is_file(&self, id: NodeId) -> bool {
        matches!(self.nodes[id as usize].kind, NodeKind::File(_))
    }

    pub fn list(&self, id: NodeId) -> FsResult<Vec<String>> {
        match &self.nodes[id as usize].kind {
            NodeKind::Dir(children) => Ok(children.iter().map(|(n, _)| n.clone()).collect()),
            NodeKind::File(_) => Err(FsError::NotADirectory),
        }
    }

    pub fn read(&self, id: NodeId) -> FsResult<&[u8]> {
        match &self.nodes[id as usize].kind {
            NodeKind::File(data) => Ok(data.as_slice()),
            NodeKind::Dir(_) => Err(FsError::IsADirectory),
        }
    }

    pub fn write(&mut self, id: NodeId, data: &[u8]) -> FsResult<()> {
        match &mut self.nodes[id as usize].kind {
            NodeKind::File(d) => {
                d.clear();
                d.extend_from_slice(data);
                Ok(())
            }
            NodeKind::Dir(_) => Err(FsError::IsADirectory),
        }
    }

    pub fn append(&mut self, id: NodeId, data: &[u8]) -> FsResult<()> {
        match &mut self.nodes[id as usize].kind {
            NodeKind::File(d) => {
                d.extend_from_slice(data);
                Ok(())
            }
            NodeKind::Dir(_) => Err(FsError::IsADirectory),
        }
    }

    /// Split a path into (parent_dir_id, final_name).
    pub fn split_parent<'a>(&self, path: &'a str) -> FsResult<(NodeId, &'a str)> {
        if path.is_empty() {
            return Err(FsError::InvalidPath);
        }
        let (parent_path, name) = match path.rfind('/') {
            None => (".", path),
            Some(0) => ("/", &path[1..]),
            Some(i) => (&path[..i], &path[i + 1..]),
        };
        if name.is_empty() || name == "." || name == ".." {
            return Err(FsError::InvalidPath);
        }
        if name.len() > MAX_NAME {
            return Err(FsError::NameTooLong);
        }
        let parent = self.resolve(parent_path)?;
        if !self.is_dir(parent) {
            return Err(FsError::NotADirectory);
        }
        Ok((parent, name))
    }

    fn insert_child(&mut self, parent: NodeId, name: &str, new_id: NodeId) -> FsResult<()> {
        match &mut self.nodes[parent as usize].kind {
            NodeKind::Dir(children) => {
                if children.iter().any(|(n, _)| n == name) {
                    return Err(FsError::AlreadyExists);
                }
                children.push((name.to_string(), new_id));
                Ok(())
            }
            NodeKind::File(_) => Err(FsError::NotADirectory),
        }
    }

    pub fn mkdir(&mut self, path: &str) -> FsResult<NodeId> {
        let (parent, name) = self.split_parent(path)?;
        let new_id = self.nodes.len() as NodeId;
        self.nodes.push(Node { kind: NodeKind::Dir(Vec::new()), parent });
        self.insert_child(parent, name, new_id)?;
        Ok(new_id)
    }

    /// Create file (must not exist).
    pub fn create_file(&mut self, path: &str, data: &[u8]) -> FsResult<NodeId> {
        let (parent, name) = self.split_parent(path)?;
        let new_id = self.nodes.len() as NodeId;
        self.nodes.push(Node { kind: NodeKind::File(data.to_vec()), parent });
        self.insert_child(parent, name, new_id)?;
        Ok(new_id)
    }

    /// Create file if missing, else overwrite contents.
    pub fn write_file(&mut self, path: &str, data: &[u8]) -> FsResult<NodeId> {
        match self.resolve(path) {
            Ok(id) => {
                self.write(id, data)?;
                Ok(id)
            }
            Err(FsError::NotFound) => self.create_file(path, data),
            Err(e) => Err(e),
        }
    }

    /// Create empty file if missing. If exists, no-op (like `touch`).
    pub fn touch(&mut self, path: &str) -> FsResult<NodeId> {
        match self.resolve(path) {
            Ok(id) => Ok(id),
            Err(FsError::NotFound) => self.create_file(path, &[]),
            Err(e) => Err(e),
        }
    }

    pub fn remove(&mut self, path: &str) -> FsResult<()> {
        let id = self.resolve(path)?;
        if id == ROOT {
            return Err(FsError::InvalidPath);
        }
        // Directories must be empty.
        if let NodeKind::Dir(children) = &self.nodes[id as usize].kind {
            if !children.is_empty() {
                return Err(FsError::NotEmpty);
            }
        }
        let parent = self.nodes[id as usize].parent;
        if let NodeKind::Dir(children) = &mut self.nodes[parent as usize].kind {
            children.retain(|(_, nid)| *nid != id);
        }
        // Tombstone the node (keep id stable).
        self.nodes[id as usize].kind = NodeKind::File(Vec::new());
        self.nodes[id as usize].parent = id; // orphan
        Ok(())
    }

    pub fn chdir(&mut self, path: &str) -> FsResult<()> {
        let id = self.resolve(path)?;
        if !self.is_dir(id) {
            return Err(FsError::NotADirectory);
        }
        self.cwd = id;
        Ok(())
    }

    pub fn cwd_path(&self) -> String {
        self.path_of(self.cwd)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn root_exists() {
        let fs = RamFs::new();
        assert!(fs.is_dir(ROOT));
        assert_eq!(fs.path_of(ROOT), "/");
    }

    #[test]
    fn mkdir_and_list() {
        let mut fs = RamFs::new();
        fs.mkdir("/a").unwrap();
        fs.mkdir("/a/b").unwrap();
        let ls_root = fs.list(ROOT).unwrap();
        assert_eq!(ls_root, alloc::vec!["a".to_string()]);
        let a = fs.resolve("/a").unwrap();
        assert_eq!(fs.list(a).unwrap(), alloc::vec!["b".to_string()]);
    }

    #[test]
    fn file_io() {
        let mut fs = RamFs::new();
        fs.create_file("/hello.txt", b"hi").unwrap();
        let id = fs.resolve("/hello.txt").unwrap();
        assert_eq!(fs.read(id).unwrap(), b"hi");
        fs.append(id, b" world").unwrap();
        assert_eq!(fs.read(id).unwrap(), b"hi world");
        fs.write(id, b"bye").unwrap();
        assert_eq!(fs.read(id).unwrap(), b"bye");
    }

    #[test]
    fn touch_idempotent() {
        let mut fs = RamFs::new();
        fs.touch("/a.txt").unwrap();
        fs.touch("/a.txt").unwrap();
        assert!(fs.is_file(fs.resolve("/a.txt").unwrap()));
    }

    #[test]
    fn cd_and_relative_paths() {
        let mut fs = RamFs::new();
        fs.mkdir("/etc").unwrap();
        fs.chdir("/etc").unwrap();
        assert_eq!(fs.cwd_path(), "/etc");
        fs.create_file("motd", b"welcome").unwrap();
        let id = fs.resolve("motd").unwrap();
        assert_eq!(fs.read(id).unwrap(), b"welcome");
        let id2 = fs.resolve("./motd").unwrap();
        assert_eq!(id, id2);
        fs.chdir("..").unwrap();
        assert_eq!(fs.cwd_path(), "/");
    }

    #[test]
    fn duplicate_errors() {
        let mut fs = RamFs::new();
        fs.mkdir("/x").unwrap();
        assert_eq!(fs.mkdir("/x"), Err(FsError::AlreadyExists));
        fs.create_file("/y", b"").unwrap();
        assert_eq!(fs.create_file("/y", b""), Err(FsError::AlreadyExists));
    }

    #[test]
    fn remove_empty_dir_and_file() {
        let mut fs = RamFs::new();
        fs.mkdir("/d").unwrap();
        fs.create_file("/d/f", b"1").unwrap();
        assert_eq!(fs.remove("/d"), Err(FsError::NotEmpty));
        fs.remove("/d/f").unwrap();
        fs.remove("/d").unwrap();
        assert_eq!(fs.resolve("/d"), Err(FsError::NotFound));
    }

    #[test]
    fn path_of_nested() {
        let mut fs = RamFs::new();
        fs.mkdir("/a").unwrap();
        fs.mkdir("/a/b").unwrap();
        let id = fs.resolve("/a/b").unwrap();
        assert_eq!(fs.path_of(id), "/a/b");
    }

    #[test]
    fn write_file_creates_or_overwrites() {
        let mut fs = RamFs::new();
        fs.write_file("/a", b"1").unwrap();
        fs.write_file("/a", b"22").unwrap();
        let id = fs.resolve("/a").unwrap();
        assert_eq!(fs.read(id).unwrap(), b"22");
    }
}
