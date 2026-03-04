package com.forgeengine.physics;

// ============================================================
//  ForgeEngine – PhysicsManager.java
//  Full Bullet physics integration via JME BulletAppState.
//  Exposed to C++ via JNI through EditorBridge.
// ============================================================

import com.jme3.bullet.BulletAppState;
import com.jme3.bullet.PhysicsSpace;
import com.jme3.bullet.collision.shapes.*;
import com.jme3.bullet.control.*;
import com.jme3.math.Vector3f;
import com.jme3.math.Quaternion;
import com.jme3.scene.Spatial;
import com.jme3.scene.Node;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

public class PhysicsManager {

    private BulletAppState  mBulletState;
    private PhysicsSpace    mPhysicsSpace;
    private final Map<Integer, RigidBodyControl>  mRigidBodies  = new HashMap<>();
    private final Map<Integer, GhostControl>      mGhostControls = new HashMap<>();
    private final Map<Integer, CharacterControl>  mCharacters    = new HashMap<>();
    private final AtomicInteger mIdGen = new AtomicInteger(1);
    private boolean mRunning = false;

    // ── Init ──────────────────────────────────────────────────
    public void init(Node rootNode) {
        mBulletState = new BulletAppState();
        // Will be attached to stateManager in ForgeJMEApp
        mPhysicsSpace = mBulletState.getPhysicsSpace();
        mPhysicsSpace.setGravity(new Vector3f(0, -9.81f, 0));
        mPhysicsSpace.setMaxSubSteps(4);
    }

    public BulletAppState getBulletAppState() { return mBulletState; }
    public PhysicsSpace   getPhysicsSpace()   { return mPhysicsSpace; }

    // ── Runtime control ───────────────────────────────────────
    public void startSimulation() {
        mBulletState.setEnabled(true);
        mRunning = true;
    }
    public void stopSimulation() {
        mBulletState.setEnabled(false);
        mRunning = false;
    }
    public void pauseSimulation() { mBulletState.setEnabled(false); }
    public void resumeSimulation(){ mBulletState.setEnabled(true);  }
    public boolean isRunning()     { return mRunning; }

    // ── Gravity ───────────────────────────────────────────────
    public void setGravity(float x, float y, float z) {
        if (mPhysicsSpace != null)
            mPhysicsSpace.setGravity(new Vector3f(x, y, z));
    }

    // ═══════════════════════════════════════════════════════
    //  RIGID BODY
    // ═══════════════════════════════════════════════════════
    public enum ShapeType {
        BOX, SPHERE, CAPSULE, CYLINDER, MESH, HULL, COMPOUND
    }

    public int addRigidBody(Spatial spatial, int shapeType,
                             float mass, float[] shapeData) {
        CollisionShape shape = buildShape(ShapeType.values()[shapeType],
                                          shapeData, spatial);
        RigidBodyControl rbc = new RigidBodyControl(shape, mass);
        spatial.addControl(rbc);
        mPhysicsSpace.add(rbc);

        int id = mIdGen.getAndIncrement();
        mRigidBodies.put(id, rbc);
        return id;
    }

    public void removeRigidBody(int id) {
        RigidBodyControl rbc = mRigidBodies.remove(id);
        if (rbc != null) {
            mPhysicsSpace.remove(rbc);
            rbc.getSpatial().removeControl(rbc);
        }
    }

    public void setMass(int id, float mass) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.setMass(mass);
    }
    public void setKinematic(int id, boolean kinematic) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.setKinematic(kinematic);
    }
    public void setFriction(int id, float friction) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.setFriction(friction);
    }
    public void setRestitution(int id, float restitution) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.setRestitution(restitution);
    }
    public void setLinearDamping(int id, float damp) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.setLinearDamping(damp);
    }
    public void setAngularDamping(int id, float damp) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.setAngularDamping(damp);
    }
    public void setLinearVelocity(int id, float x, float y, float z) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.setLinearVelocity(new Vector3f(x,y,z));
    }
    public void applyImpulse(int id, float x, float y, float z) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.applyCentralImpulse(new Vector3f(x,y,z));
    }
    public void applyForce(int id, float x, float y, float z) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc != null) rbc.applyCentralForce(new Vector3f(x,y,z));
    }

    // Get physics transform for sync back to scene
    public float[] getRigidBodyTransform(int id) {
        RigidBodyControl rbc = mRigidBodies.get(id);
        if (rbc == null) return new float[10];
        Vector3f t = rbc.getPhysicsLocation(null);
        Quaternion q = rbc.getPhysicsRotation(null);
        Vector3f s = rbc.getSpatial().getLocalScale();
        return new float[]{t.x,t.y,t.z, q.getX(),q.getY(),q.getZ(),q.getW(), s.x,s.y,s.z};
    }

    // ═══════════════════════════════════════════════════════
    //  CHARACTER CONTROLLER
    // ═══════════════════════════════════════════════════════
    public int addCharacterController(Spatial spatial,
                                       float radius, float height) {
        CapsuleCollisionShape capsule = new CapsuleCollisionShape(radius, height);
        CharacterControl cc = new CharacterControl(capsule, 0.05f);
        cc.setJumpSpeed(10f);
        cc.setFallSpeed(30f);
        cc.setGravity(30f);
        spatial.addControl(cc);
        mPhysicsSpace.add(cc);
        int id = mIdGen.getAndIncrement();
        mCharacters.put(id, cc);
        return id;
    }
    public void setWalkDirection(int id, float x, float y, float z) {
        CharacterControl cc = mCharacters.get(id);
        if (cc != null) cc.setWalkDirection(new Vector3f(x,y,z));
    }
    public void characterJump(int id) {
        CharacterControl cc = mCharacters.get(id);
        if (cc != null) cc.jump();
    }
    public boolean isOnGround(int id) {
        CharacterControl cc = mCharacters.get(id);
        return cc != null && cc.onGround();
    }

    // ═══════════════════════════════════════════════════════
    //  RAYCASTING
    // ═══════════════════════════════════════════════════════
    // Returns [hitX, hitY, hitZ, normalX, normalY, normalZ, hitSpatialId]
    public float[] raycast(float ox, float oy, float oz,
                            float dx, float dy, float dz,
                            float maxDist) {
        Vector3f from = new Vector3f(ox,oy,oz);
        Vector3f dir  = new Vector3f(dx,dy,dz).normalizeLocal();
        Vector3f to   = from.add(dir.mult(maxDist));

        var results = mPhysicsSpace.rayTest(from, to);
        if (results.isEmpty()) return new float[0];

        var hit = results.get(0);
        Vector3f hp = from.interpolateLocal(to, hit.getHitFraction());
        Vector3f hn = hit.getHitNormalLocal(null);
        // Resolve spatial ID
        Object obj = hit.getCollisionObject().getUserObject();
        int spatId = -1;
        if (obj instanceof Spatial)
            spatId = ((Spatial)obj).getUserData("forge_id") != null
                ? (int)(Integer)((Spatial)obj).getUserData("forge_id") : -1;
        return new float[]{hp.x,hp.y,hp.z, hn.x,hn.y,hn.z, spatId};
    }

    // ═══════════════════════════════════════════════════════
    //  HELPER: Build collision shape
    // ═══════════════════════════════════════════════════════
    private CollisionShape buildShape(ShapeType type,
                                       float[] data, Spatial spatial) {
        switch(type) {
        case SPHERE:
            return new SphereCollisionShape(data.length>0 ? data[0] : 0.5f);
        case CAPSULE:
            return new CapsuleCollisionShape(
                data.length>0?data[0]:0.25f,
                data.length>1?data[1]:1f);
        case CYLINDER:
            return new CylinderCollisionShape(
                new Vector3f(
                    data.length>0?data[0]:0.25f,
                    data.length>1?data[1]:0.5f,
                    data.length>0?data[0]:0.25f));
        case MESH:
            return new MeshCollisionShape(
                ((com.jme3.scene.Geometry)spatial).getMesh());
        case HULL:
            return new HullCollisionShape(
                ((com.jme3.scene.Geometry)spatial).getMesh());
        case BOX:
        default:
            return new BoxCollisionShape(new Vector3f(
                data.length>0?data[0]:0.5f,
                data.length>1?data[1]:0.5f,
                data.length>2?data[2]:0.5f));
        }
    }
}
