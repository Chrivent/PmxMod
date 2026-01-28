package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.WatchEvent;
import java.nio.file.WatchKey;
import java.nio.file.WatchService;
import java.nio.file.StandardWatchEventKinds;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.function.Predicate;

public final class PmxFileWatcher {
    private final Path root;
    private final int registerDepth;
    private final boolean recursive;
    private final Predicate<Path> match;
    private final Runnable onChange;
    private final Set<Path> watchedDirs = new HashSet<>();
    private WatchService watchService;
    private Thread watchThread;

    public PmxFileWatcher(Path root, int registerDepth, boolean recursive,
                          Predicate<Path> match, Runnable onChange) {
        this.root = root;
        this.registerDepth = registerDepth;
        this.recursive = recursive;
        this.match = match;
        this.onChange = onChange;
    }

    public void start() {
        stop();
        if (root == null) return;
        try {
            watchService = root.getFileSystem().newWatchService();
            registerDir(root);
            if (registerDepth > 0) {
                try (var stream = Files.walk(root, registerDepth)) {
                    stream.filter(Files::isDirectory).forEach(this::registerDir);
                } catch (Exception ignored) {
                }
            }
        } catch (Exception ignored) {
            return;
        }
        watchThread = new Thread(this::watchLoop, "pmx-file-watch");
        watchThread.setDaemon(true);
        watchThread.start();
    }

    public void stop() {
        if (watchThread != null) {
            watchThread.interrupt();
            watchThread = null;
        }
        if (watchService != null) {
            try { watchService.close(); } catch (Exception ignored) {}
            watchService = null;
        }
        watchedDirs.clear();
    }

    private void registerDir(Path dir) {
        if (dir == null || watchedDirs.contains(dir)) return;
        try {
            dir.register(watchService,
                    StandardWatchEventKinds.ENTRY_CREATE,
                    StandardWatchEventKinds.ENTRY_DELETE,
                    StandardWatchEventKinds.ENTRY_MODIFY);
            watchedDirs.add(dir);
        } catch (Exception ignored) {
        }
    }

    private void watchLoop() {
        while (watchService != null) {
            try {
                WatchKey key = watchService.poll(250, TimeUnit.MILLISECONDS);
                if (key == null) continue;
                Path dir = (Path) key.watchable();
                boolean changed = false;
                for (WatchEvent<?> event : key.pollEvents()) {
                    WatchEvent.Kind<?> kind = event.kind();
                    if (kind == StandardWatchEventKinds.OVERFLOW) continue;
                    @SuppressWarnings("unchecked")
                    WatchEvent<Path> ev = (WatchEvent<Path>) event;
                    Path child = dir.resolve(ev.context());
                    if (recursive && Files.isDirectory(child) && kind == StandardWatchEventKinds.ENTRY_CREATE) {
                        registerDir(child);
                    }
                    if (match == null || match.test(child)) {
                        changed = true;
                    }
                }
                key.reset();
                if (changed) {
                    onChange.run();
                }
            } catch (InterruptedException ignored) {
                return;
            } catch (Exception ignored) {
            }
        }
    }
}
