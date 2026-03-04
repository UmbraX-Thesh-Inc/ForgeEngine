package com.forgeengine;

import java.util.ArrayDeque;
import java.util.Deque;
import java.util.function.Runnable;

// ============================================================
//  ForgeCommandStack.java  –  Undo / Redo command pattern
// ============================================================

public class ForgeCommandStack {

    public interface Command {
        void execute();
        void undo();
        String description();
    }

    private final Deque<Command> mUndo = new ArrayDeque<>();
    private final Deque<Command> mRedo = new ArrayDeque<>();
    private static final int MAX = 64;

    public void push(Command cmd) {
        cmd.execute();
        mUndo.push(cmd);
        mRedo.clear();
        if (mUndo.size() > MAX) mUndo.pollLast();
    }

    public void undo() {
        if (mUndo.isEmpty()) return;
        Command cmd = mUndo.pop();
        cmd.undo();
        mRedo.push(cmd);
    }

    public void redo() {
        if (mRedo.isEmpty()) return;
        Command cmd = mRedo.pop();
        cmd.execute();
        mUndo.push(cmd);
    }

    public boolean canUndo() { return !mUndo.isEmpty(); }
    public boolean canRedo() { return !mRedo.isEmpty(); }
    public String  peekUndo() { return mUndo.isEmpty() ? "" : mUndo.peek().description(); }
    public String  peekRedo() { return mRedo.isEmpty() ? "" : mRedo.peek().description(); }
}
