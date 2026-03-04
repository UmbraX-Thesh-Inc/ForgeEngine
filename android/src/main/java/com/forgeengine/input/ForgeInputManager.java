package com.forgeengine.input;

// ============================================================
//  ForgeEngine – ForgeInputManager.java
//  Unified input: touch gestures, virtual joystick, keyboard,
//  gamepad — dispatches to JME InputManager AND native ImGui.
// ============================================================

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import com.jme3.input.controls.*;
import com.jme3.input.InputManager;
import com.jme3.input.KeyInput;
import com.jme3.input.TouchInput;
import com.jme3.math.Vector2f;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

public class ForgeInputManager {

    // ── Action registry ───────────────────────────────────────
    public interface ActionListener { void onAction(String name, boolean pressed); }
    public interface AxisListener   { void onAxis(String name, float value); }

    private final Map<String, List<ActionListener>> mActionListeners = new ConcurrentHashMap<>();
    private final Map<String, List<AxisListener>>   mAxisListeners   = new ConcurrentHashMap<>();

    // ── State ──────────────────────────────────────────────────
    private final Map<String,  Boolean> mKeys      = new ConcurrentHashMap<>();
    private final Map<Integer, float[]> mTouches   = new ConcurrentHashMap<>();
    // Gamepad axes
    private float mLeftX=0, mLeftY=0, mRightX=0, mRightY=0;
    private float mLT=0, mRT=0;

    // ── JME InputManager reference ────────────────────────────
    private InputManager mJmeInput;

    // ── Virtual joystick positions (for on-screen display) ────
    public float leftJoyX=0, leftJoyY=0, rightJoyX=0, rightJoyY=0;
    private float leftJoyRadius=80.f, rightJoyRadius=80.f;
    private float[] leftJoyCenter  = {200.f, 800.f};
    private float[] rightJoyCenter = {1100.f, 800.f};
    private int  leftTouchId=-1, rightTouchId=-1;

    // ─────────────────────────────────────────────────────────
    //  Init
    // ─────────────────────────────────────────────────────────
    public void init(InputManager jmeInput) {
        mJmeInput = jmeInput;
        setupDefaultMappings();
    }

    private void setupDefaultMappings() {
        if (mJmeInput == null) return;
        // Movement
        mJmeInput.addMapping("MoveForward",  new KeyTrigger(KeyInput.KEY_W));
        mJmeInput.addMapping("MoveBackward", new KeyTrigger(KeyInput.KEY_S));
        mJmeInput.addMapping("MoveLeft",     new KeyTrigger(KeyInput.KEY_A));
        mJmeInput.addMapping("MoveRight",    new KeyTrigger(KeyInput.KEY_D));
        mJmeInput.addMapping("Jump",         new KeyTrigger(KeyInput.KEY_SPACE));
        mJmeInput.addMapping("Sprint",       new KeyTrigger(KeyInput.KEY_LSHIFT));
        mJmeInput.addMapping("Crouch",       new KeyTrigger(KeyInput.KEY_LCONTROL));
        mJmeInput.addMapping("Interact",     new KeyTrigger(KeyInput.KEY_E));
        mJmeInput.addMapping("Fire",         new KeyTrigger(KeyInput.KEY_F));

        // Touch mappings
        mJmeInput.addMapping("TouchTap",     new TouchTrigger(TouchInput.ALL));
    }

    // ─────────────────────────────────────────────────────────
    //  Touch event processing
    // ─────────────────────────────────────────────────────────
    public boolean onTouchEvent(MotionEvent event, int screenW, int screenH) {
        int action = event.getActionMasked();
        int ptrIdx = event.getActionIndex();
        int ptrId  = event.getPointerId(ptrIdx);
        float x    = event.getX(ptrIdx);
        float y    = event.getY(ptrIdx);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                mTouches.put(ptrId, new float[]{x, y, 0.f});
                processVirtualJoystick(ptrId, x, y, true, screenW, screenH);
                break;

            case MotionEvent.ACTION_MOVE:
                for (int i=0; i<event.getPointerCount(); i++) {
                    int pid = event.getPointerId(i);
                    float px = event.getX(i), py = event.getY(i);
                    mTouches.put(pid, new float[]{px, py, 0.f});
                    if (pid==leftTouchId)  updateJoy(px,py,true);
                    if (pid==rightTouchId) updateJoy(px,py,false);
                }
                break;

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL:
                mTouches.remove(ptrId);
                if (ptrId==leftTouchId)  { leftJoyX=0;leftJoyY=0;  leftTouchId=-1;  }
                if (ptrId==rightTouchId) { rightJoyX=0;rightJoyY=0; rightTouchId=-1; }
                fireAxis("MoveForwardBack",  leftJoyY);
                fireAxis("MoveSideways",     leftJoyX);
                fireAxis("LookHorizontal",   rightJoyX);
                fireAxis("LookVertical",     rightJoyY);
                break;
        }
        return false; // let ImGui handle the rest
    }

    private void processVirtualJoystick(int pid, float x, float y,
                                          boolean down, int sw, int sh) {
        // Left joystick: left half of screen
        if (x < sw/2f && leftTouchId < 0) {
            leftJoyCenter[0] = x; leftJoyCenter[1] = y;
            leftTouchId = pid;
        }
        // Right joystick: right half
        else if (x >= sw/2f && rightTouchId < 0) {
            rightJoyCenter[0] = x; rightJoyCenter[1] = y;
            rightTouchId = pid;
        }
    }

    private void updateJoy(float x, float y, boolean isLeft) {
        float[] center = isLeft ? leftJoyCenter : rightJoyCenter;
        float r = isLeft ? leftJoyRadius : rightJoyRadius;
        float dx = x - center[0], dy = y - center[1];
        float dist = (float)Math.sqrt(dx*dx+dy*dy);
        if (dist > r) { float s=r/dist; dx*=s; dy*=s; }
        float nx = dx/r, ny = dy/r;
        if (isLeft) {
            leftJoyX=nx; leftJoyY=-ny; // Y inverted (up=positive)
            fireAxis("MoveSideways",    nx);
            fireAxis("MoveForwardBack", -ny);
        } else {
            rightJoyX=nx; rightJoyY=-ny;
            fireAxis("LookHorizontal", nx);
            fireAxis("LookVertical",   -ny);
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Gamepad
    // ─────────────────────────────────────────────────────────
    public boolean onGenericMotionEvent(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) != 0) {
            mLeftX  = getCenteredAxis(event, MotionEvent.AXIS_X);
            mLeftY  = getCenteredAxis(event, MotionEvent.AXIS_Y);
            mRightX = getCenteredAxis(event, MotionEvent.AXIS_Z);
            mRightY = getCenteredAxis(event, MotionEvent.AXIS_RZ);
            mLT     = getCenteredAxis(event, MotionEvent.AXIS_LTRIGGER);
            mRT     = getCenteredAxis(event, MotionEvent.AXIS_RTRIGGER);
            fireAxis("MoveSideways",    mLeftX);
            fireAxis("MoveForwardBack", -mLeftY);
            fireAxis("LookHorizontal",  mRightX);
            fireAxis("LookVertical",    -mRightY);
            fireAxis("TriggerLeft",     mLT);
            fireAxis("TriggerRight",    mRT);
            return true;
        }
        return false;
    }
    private float getCenteredAxis(MotionEvent e, int axis) {
        InputDevice.MotionRange range =
            e.getDevice().getMotionRange(axis, e.getSource());
        if (range == null) return 0.f;
        float v = e.getAxisValue(axis);
        if (Math.abs(v) <= 0.1f) return 0.f; // dead zone
        return v;
    }

    // ─────────────────────────────────────────────────────────
    //  Key events
    // ─────────────────────────────────────────────────────────
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        String name = KeyEvent.keyCodeToString(keyCode);
        mKeys.put(name, true);
        fireAction(name, true);
        return false;
    }
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        String name = KeyEvent.keyCodeToString(keyCode);
        mKeys.put(name, false);
        fireAction(name, false);
        return false;
    }

    // ─────────────────────────────────────────────────────────
    //  Listener registration
    // ─────────────────────────────────────────────────────────
    public void addActionListener(String mapping, ActionListener l) {
        mActionListeners.computeIfAbsent(mapping, k->new ArrayList<>()).add(l);
    }
    public void addAxisListener(String mapping, AxisListener l) {
        mAxisListeners.computeIfAbsent(mapping, k->new ArrayList<>()).add(l);
    }
    public void removeActionListener(String mapping, ActionListener l) {
        List<ActionListener> ls = mActionListeners.get(mapping);
        if (ls != null) ls.remove(l);
    }

    // ─────────────────────────────────────────────────────────
    //  Fire events
    // ─────────────────────────────────────────────────────────
    private void fireAction(String name, boolean pressed) {
        List<ActionListener> ls = mActionListeners.get(name);
        if (ls != null) for (ActionListener l : ls) l.onAction(name, pressed);
    }
    private void fireAxis(String name, float value) {
        List<AxisListener> ls = mAxisListeners.get(name);
        if (ls != null) for (AxisListener l : ls) l.onAxis(name, value);
    }

    // ─────────────────────────────────────────────────────────
    //  Query
    // ─────────────────────────────────────────────────────────
    public boolean isKeyDown(String mapping) {
        return Boolean.TRUE.equals(mKeys.get(mapping));
    }
    public int getTouchCount() { return mTouches.size(); }
    public float[] getTouchPos(int index) {
        return mTouches.values().stream().skip(index).findFirst().orElse(null);
    }

    // Virtual joystick data for ImGui overlay
    public float[] getLeftJoyCenterAndRadius() {
        return new float[]{leftJoyCenter[0],leftJoyCenter[1],leftJoyRadius};
    }
    public float[] getRightJoyCenterAndRadius() {
        return new float[]{rightJoyCenter[0],rightJoyCenter[1],rightJoyRadius};
    }
}
