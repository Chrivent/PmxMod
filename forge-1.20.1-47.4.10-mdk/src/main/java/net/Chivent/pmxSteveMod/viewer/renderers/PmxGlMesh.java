package net.Chivent.pmxSteveMod.viewer.renderers;

import net.Chivent.pmxSteveMod.jni.PmxNative;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import org.lwjgl.opengl.GL11C;
import org.lwjgl.opengl.GL15C;
import org.lwjgl.opengl.GL20C;
import org.lwjgl.opengl.GL30C;

import java.nio.ByteBuffer;

final class PmxGlMesh {
    static final int LOC_POS = 0;
    static final int LOC_COLOR = 1;
    static final int LOC_UV0 = 2;
    static final int LOC_UV1 = 3;
    static final int LOC_UV2 = 4;
    static final int LOC_NRM = 5;

    long ownerHandle = 0L;

    int vao = 0;
    int vboPos = 0;
    int vboNrm = 0;
    int vboUv  = 0;
    int ibo    = 0;

    int vertexCount = 0;
    int indexCount  = 0;
    int elemSize    = 0;
    int glIndexType = GL11C.GL_UNSIGNED_INT;

    boolean ready = false;

    void ensure(PmxInstance instance) {
        long h = instance.handle();
        if (h == 0L) return;

        int vtxCount = PmxNative.nativeGetVertexCount(h);
        int idxCount = PmxNative.nativeGetIndexCount(h);
        int elemSize = PmxNative.nativeGetIndexElementSize(h);
        int glType = toGlIndexType(elemSize);

        if (ready
                && ownerHandle == h
                && vertexCount == vtxCount
                && indexCount == idxCount
                && this.elemSize == elemSize
                && glIndexType == glType) {
            return;
        }

        destroy();

        ByteBuffer idx = instance.idxBuf();
        if (idx == null || vtxCount <= 0 || idxCount <= 0 || elemSize <= 0) return;

        ownerHandle = h;
        vertexCount = vtxCount;
        indexCount = idxCount;
        this.elemSize = elemSize;
        glIndexType = glType;

        vao = GL30C.glGenVertexArrays();
        vboPos = GL15C.glGenBuffers();
        vboNrm = GL15C.glGenBuffers();
        vboUv  = GL15C.glGenBuffers();
        ibo    = GL15C.glGenBuffers();

        GL30C.glBindVertexArray(vao);

        setupFloatVbo(vboPos, LOC_POS, 3, (long) vtxCount * 3L * 4L);
        setupFloatVbo(vboUv, LOC_UV0, 2, (long) vtxCount * 2L * 4L);
        setupFloatVbo(vboNrm, LOC_NRM, 3, (long) vtxCount * 3L * 4L);

        GL20C.glDisableVertexAttribArray(LOC_COLOR);
        GL20C.glDisableVertexAttribArray(LOC_UV1);
        GL20C.glDisableVertexAttribArray(LOC_UV2);

        GL15C.glBindBuffer(GL15C.GL_ELEMENT_ARRAY_BUFFER, ibo);
        ByteBuffer idxDup = idx.duplicate();
        idxDup.rewind();
        GL15C.glBufferData(GL15C.GL_ELEMENT_ARRAY_BUFFER, idxDup, GL15C.GL_STATIC_DRAW);

        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, 0);
        GL30C.glBindVertexArray(0);

        ready = true;
    }

    void destroy() {
        vao = deleteVao(vao);
        vboPos = deleteBuffer(vboPos);
        vboNrm = deleteBuffer(vboNrm);
        vboUv = deleteBuffer(vboUv);
        ibo = deleteBuffer(ibo);
        ready = false;
        ownerHandle = 0L;
        vertexCount = 0;
        indexCount = 0;
        elemSize = 0;
        glIndexType = GL11C.GL_UNSIGNED_INT;
    }

    void updateDynamic(PmxInstance instance) {
        int vtxCount = vertexCount;
        uploadDynamic(vboPos, instance.posBuf(), (long) vtxCount * 3L * 4L);
        uploadDynamic(vboNrm, instance.nrmBuf(), (long) vtxCount * 3L * 4L);
        uploadDynamic(vboUv,  instance.uvBuf(),  (long) vtxCount * 2L * 4L);
    }

    private static void uploadDynamic(int vbo, ByteBuffer src, long expectedBytes) {
        if (vbo == 0 || src == null) return;
        if (src.capacity() < expectedBytes) return;

        ByteBuffer dup = src.duplicate();
        dup.rewind();

        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, vbo);
        GL15C.glBufferSubData(GL15C.GL_ARRAY_BUFFER, 0L, dup);
        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, 0);
    }

    private static void setupFloatVbo(int vbo, int loc, int size, long bytes) {
        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, vbo);
        GL15C.glBufferData(GL15C.GL_ARRAY_BUFFER, bytes, GL15C.GL_DYNAMIC_DRAW);
        GL20C.glEnableVertexAttribArray(loc);
        GL20C.glVertexAttribPointer(loc, size, GL11C.GL_FLOAT, false, 0, 0L);
    }

    private static int deleteBuffer(int id) {
        if (id != 0) GL15C.glDeleteBuffers(id);
        return 0;
    }

    private static int deleteVao(int id) {
        if (id != 0) GL30C.glDeleteVertexArrays(id);
        return 0;
    }

    private static int toGlIndexType(int elemSize) {
        return switch (elemSize) {
            case 1 -> GL11C.GL_UNSIGNED_BYTE;
            case 2 -> GL11C.GL_UNSIGNED_SHORT;
            default -> GL11C.GL_UNSIGNED_INT;
        };
    }
}
