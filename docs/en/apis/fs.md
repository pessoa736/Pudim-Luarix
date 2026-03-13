# API fs (kfs)

Global tables: fs and kfs (alias)

Calls:

- fs.write(name, content)
- fs.append(name, content)
- fs.read(name)
- fs.delete(name)
- fs.exists(name)
- fs.size(name)
- fs.count()
- fs.list()
- fs.clear()
- fs.save()
- fs.load()
- fs.persistent()
- fs.chmod(name, mode)
- fs.chown(name, uid, gid)
- fs.getperm(name)

Permission checks are enforced by kernel security capabilities.
