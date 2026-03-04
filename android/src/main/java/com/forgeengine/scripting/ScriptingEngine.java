package com.forgeengine.scripting;

// ============================================================
//  ForgeEngine – ScriptingEngine.java
//  Runtime scripting via Groovy (or BeanShell fallback).
//  Scripts are Java-compatible so the user writes real Java/Groovy.
//  Each spatial can have N scripts attached as ScriptControl.
// ============================================================

import com.jme3.scene.Spatial;
import com.jme3.scene.control.AbstractControl;
import com.jme3.renderer.RenderManager;
import com.jme3.renderer.ViewPort;

import java.io.*;
import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

public class ScriptingEngine {

    // ── Script registry ───────────────────────────────────────
    public static class ScriptEntry {
        public int    id;
        public String name;
        public String path;
        public String source;
        public boolean enabled = true;
        public String lastError = "";
    }

    private final Map<Integer, ScriptEntry>        mScripts   = new ConcurrentHashMap<>();
    private final Map<Integer, List<Integer>>      mAttached  = new ConcurrentHashMap<>();
    // scriptId -> compiled class (via reflection on Groovy classloader)
    private final Map<Integer, Class<?>>           mCompiled  = new ConcurrentHashMap<>();
    private final AtomicInteger mIdGen = new AtomicInteger(1);

    private ClassLoader mGroovyLoader;
    private boolean     mGroovyAvailable = false;

    // ── Script lifecycle callbacks (called from ScriptControl) ─
    public interface ScriptCallbacks {
        void onInit(Object scriptInstance, Spatial spatial);
        void onUpdate(Object scriptInstance, float tpf);
        void onDestroy(Object scriptInstance);
    }

    // ─────────────────────────────────────────────────────────
    //  Init – detect Groovy at runtime
    // ─────────────────────────────────────────────────────────
    public void init() {
        try {
            Class<?> gcl = Class.forName("groovy.lang.GroovyClassLoader");
            mGroovyLoader = (ClassLoader) gcl.getConstructor().newInstance();
            mGroovyAvailable = true;
        } catch (Exception e) {
            // Groovy not bundled – use interpreted fallback
            mGroovyAvailable = false;
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Register script source
    // ─────────────────────────────────────────────────────────
    public int registerScript(String name, String path, String source) {
        ScriptEntry e = new ScriptEntry();
        e.id     = mIdGen.getAndIncrement();
        e.name   = name;
        e.path   = path;
        e.source = source;
        mScripts.put(e.id, e);
        return e.id;
    }

    public int loadScriptFromFile(String path) throws IOException {
        File f = new File(path);
        StringBuilder sb = new StringBuilder();
        try (BufferedReader br = new BufferedReader(new FileReader(f))) {
            String line;
            while ((line = br.readLine()) != null) {
                sb.append(line).append('\n');
            }
        }
        String name = f.getName().replace(".java","").replace(".groovy","");
        return registerScript(name, path, sb.toString());
    }

    public void updateScriptSource(int scriptId, String newSource) {
        ScriptEntry e = mScripts.get(scriptId);
        if (e != null) {
            e.source = newSource;
            mCompiled.remove(scriptId); // force recompile
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Compile
    // ─────────────────────────────────────────────────────────
    public boolean compileScript(int scriptId) {
        ScriptEntry e = mScripts.get(scriptId);
        if (e == null) return false;

        if (!mGroovyAvailable) {
            e.lastError = "Groovy not available – using interpreted mode";
            return false;
        }

        try {
            // GroovyClassLoader.parseClass(String)
            Class<?> gcl = mGroovyLoader.getClass();
            Method parseClass = gcl.getMethod("parseClass", String.class);
            Class<?> compiled = (Class<?>) parseClass.invoke(mGroovyLoader, e.source);
            mCompiled.put(scriptId, compiled);
            e.lastError = "";
            return true;
        } catch (Exception ex) {
            e.lastError = ex.getMessage() != null ? ex.getMessage() : ex.toString();
            return false;
        }
    }

    public String getCompileError(int scriptId) {
        ScriptEntry e = mScripts.get(scriptId);
        return e != null ? e.lastError : "";
    }

    // ─────────────────────────────────────────────────────────
    //  Attach script to spatial
    // ─────────────────────────────────────────────────────────
    public ScriptControl attachScript(int scriptId, Spatial spatial) {
        ScriptEntry entry = mScripts.get(scriptId);
        if (entry == null) return null;

        // Ensure compiled
        if (!mCompiled.containsKey(scriptId)) compileScript(scriptId);

        ScriptControl ctrl = new ScriptControl(entry, mCompiled.get(scriptId));
        spatial.addControl(ctrl);
        mAttached.computeIfAbsent(
            (Integer)(Integer)spatial.getUserData("forge_id"),
            k -> new ArrayList<>()).add(scriptId);
        return ctrl;
    }

    public void detachScript(int scriptId, Spatial spatial) {
        for (int i = 0; i < spatial.getNumControls(); i++) {
            if (spatial.getControl(i) instanceof ScriptControl sc) {
                if (sc.getScriptId() == scriptId) {
                    spatial.removeControl(sc);
                    break;
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Script list JSON (for inspector)
    // ─────────────────────────────────────────────────────────
    public String getScriptListJSON() {
        StringBuilder sb = new StringBuilder("[");
        boolean first = true;
        for (ScriptEntry e : mScripts.values()) {
            if (!first) sb.append(",");
            first = false;
            sb.append("{\"id\":").append(e.id)
              .append(",\"name\":\"").append(e.name).append("\"")
              .append(",\"path\":\"").append(e.path).append("\"")
              .append(",\"enabled\":").append(e.enabled)
              .append(",\"compiled\":").append(mCompiled.containsKey(e.id))
              .append(",\"error\":\"").append(e.lastError.replace("\"","'")).append("\"")
              .append("}");
        }
        sb.append("]");
        return sb.toString();
    }

    public List<Integer> getAttachedScripts(int spatialId) {
        return mAttached.getOrDefault(spatialId, Collections.emptyList());
    }

    // ─────────────────────────────────────────────────────────
    //  ScriptControl – JME Control wrapping a script instance
    // ─────────────────────────────────────────────────────────
    public static class ScriptControl extends AbstractControl {
        private final ScriptEntry mEntry;
        private final Class<?>    mClass;
        private Object            mInstance;
        private Method            mOnInit, mOnUpdate, mOnDestroy;
        private boolean           mInitDone = false;

        public ScriptControl(ScriptEntry entry, Class<?> compiled) {
            mEntry = entry;
            mClass = compiled;
        }

        public int getScriptId() { return mEntry.id; }

        @Override
        public void setSpatial(Spatial sp) {
            super.setSpatial(sp);
            if (sp != null && mClass != null && !mInitDone) {
                try {
                    mInstance = mClass.getConstructor().newInstance();
                    // Cache lifecycle methods (optional – script may not have all)
                    try { mOnInit    = mClass.getMethod("onInit", Spatial.class); } catch(NoSuchMethodException ignored){}
                    try { mOnUpdate  = mClass.getMethod("onUpdate", float.class); } catch(NoSuchMethodException ignored){}
                    try { mOnDestroy = mClass.getMethod("onDestroy"); }              catch(NoSuchMethodException ignored){}
                    // Inject spatial reference if field exists
                    try {
                        Field f = mClass.getField("spatial");
                        f.set(mInstance, sp);
                    } catch(Exception ignored){}
                    if (mOnInit != null) mOnInit.invoke(mInstance, sp);
                    mInitDone = true;
                } catch (Exception e) {
                    mEntry.lastError = e.getMessage();
                }
            }
            if (sp == null && mInstance != null) {
                try { if (mOnDestroy != null) mOnDestroy.invoke(mInstance); }
                catch (Exception ignored) {}
            }
        }

        @Override
        protected void controlUpdate(float tpf) {
            if (!mEntry.enabled || mInstance == null || mOnUpdate == null) return;
            try { mOnUpdate.invoke(mInstance, tpf); }
            catch (Exception e) { mEntry.lastError = e.getMessage(); }
        }

        @Override
        protected void controlRender(RenderManager rm, ViewPort vp) {}
    }
}
