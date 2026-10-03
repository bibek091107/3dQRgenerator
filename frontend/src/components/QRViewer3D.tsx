"use client";

/**
 * 3D preview reproducing the desktop OpenGL renderer.
 *
 * The mesh, its per-vertex normals and the black/white module colors all come
 * from the C++ engine (served as raw float32 by /api/objects/{id}/mesh), and the
 * fragment shader is a direct port of Renderer.cpp so the preview looks the same
 * as the native app.
 */

import { OrbitControls } from "@react-three/drei";
import { Canvas } from "@react-three/fiber";
import { useEffect, useMemo, useState } from "react";
import * as THREE from "three";

import { api } from "@/lib/api";

/** Ported from Renderer.cpp: layout 0 = position, 1 = normal, 2 = color. */
const VERTEX_SHADER = /* glsl */ `
  attribute vec3 aNormal;
  attribute vec3 aColor;
  varying vec3 vNormal;
  varying vec3 vColor;

  void main() {
    vNormal = normalMatrix * aNormal;
    vColor = aColor;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`;

const FRAGMENT_SHADER = /* glsl */ `
  varying vec3 vNormal;
  varying vec3 vColor;

  void main() {
    // Separate black (QR module) vs white (base) paths.
    // Color comes from vertex data: 0,0,0 = module; 1,1,1 = base.
    float brightness = vColor.r + vColor.g + vColor.b;

    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 result;
    if (brightness < 0.5) {
      // BLACK QR modules - keep them dark, only mild ambient
      result = vec3(0.04) + diff * vec3(0.06);
    } else {
      // WHITE BASE - near-white with gentle shading
      result = vec3(0.55) + diff * vec3(0.45);
      result = clamp(result, vec3(0.0), vec3(1.0));
    }
    gl_FragColor = vec4(result, 1.0);
  }
`;

type MeshStats = { vertexCount: number; triangles: number; floats: number };

/** Splits the engine's 9-float interleaved vertex into position/normal/color. */
function buildGeometry(interleaved: Float32Array): THREE.BufferGeometry {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute("position", new THREE.BufferAttribute(interleaved.filter((_, i) => i % 9 < 3), 3));
  geo.setAttribute("aNormal", new THREE.BufferAttribute(interleaved.filter((_, i) => i % 9 >= 3 && i % 9 < 6), 3));
  geo.setAttribute("aColor", new THREE.BufferAttribute(interleaved.filter((_, i) => i % 9 >= 6), 3));
  return geo;
}

function QRMesh({ objectId, sizeMM }: { objectId: string; sizeMM: number }) {
  // Everything is keyed by objectId so a stale mesh from the previous QR is
  // never shown while the next one loads.
  const [mesh, setMesh] = useState<{
    objectId: string;
    geometry: THREE.BufferGeometry;
    stats: MeshStats;
  } | null>(null);
  const [error, setError] = useState<{ objectId: string; message: string } | null>(null);

  const material = useMemo(
    () =>
      new THREE.ShaderMaterial({
        vertexShader: VERTEX_SHADER,
        fragmentShader: FRAGMENT_SHADER,
      }),
    [],
  );

  useEffect(() => () => material.dispose(), [material]);

  // Camera::Reset() spherical -> cartesian, targeting the plate centre so all
  // four size options stay framed (identical to (4.5, 4.5, 0) at 9mm).
  const { position, target } = useMemo(() => {
    const half = sizeMM / 2;
    const d = 15;
    const p = (45 * Math.PI) / 180;
    const y = (45 * Math.PI) / 180;
    return {
      position: [
        half + d * Math.cos(p) * Math.sin(y),
        half + d * Math.sin(p),
        half + d * Math.cos(p) * Math.cos(y),
      ] as [number, number, number],
      target: new THREE.Vector3(half, half, 0),
    };
  }, [sizeMM]);

  useEffect(() => {
    let cancelled = false;
    const ctrl = new AbortController();

    (async () => {
      try {
        const res = await fetch(api.meshUrl(objectId), {
          signal: ctrl.signal,
          cache: "no-store",
        });
        if (!res.ok) {
          const body = (await res.json().catch(() => null)) as {
            detail?: { message?: string };
          } | null;
          throw new Error(body?.detail?.message ?? `HTTP ${res.status}`);
        }
        // Raw float32, 9 floats per vertex (px py pz nx ny nz cr cg cb).
        const buffer = new Float32Array(await res.arrayBuffer());
        if (cancelled) return;
        const vertexCount = Math.floor(buffer.length / 9);
        setMesh({
          objectId,
          geometry: buildGeometry(buffer),
          stats: { vertexCount, triangles: Math.floor(vertexCount / 3), floats: buffer.length },
        });
      } catch (e) {
        if (!cancelled) {
          setError({ objectId, message: e instanceof Error ? e.message : String(e) });
        }
      }
    })();

    return () => {
      cancelled = true;
      ctrl.abort();
    };
  }, [objectId]);

  useEffect(() => () => mesh?.geometry.dispose(), [mesh]);

  if (error?.objectId === objectId) {
    return (
      <div className="flex h-full items-center justify-center p-6 text-center text-sm text-[#ff8080]">
        {error.message}
      </div>
    );
  }

  const ready = mesh?.objectId === objectId ? mesh : null;

  return (
    <>
      <Canvas
        camera={{ fov: 45, near: 0.1, far: 100, position }}
        onCreated={({ camera }) => camera.lookAt(target)}
      >
        {/* glClearColor(0.15, 0.15, 0.18) */}
        <color attach="background" args={["#26262e"]} />
        {ready ? <mesh geometry={ready.geometry} material={material} /> : null}
        <OrbitControls
          target={target}
          minDistance={1}
          maxDistance={50}
          minPolarAngle={0}
          // Camera clamps pitch to +/-89 degrees.
          maxPolarAngle={Math.PI - 0.0175}
          enableDamping
          dampingFactor={0.1}
        />
      </Canvas>
      <div className="pointer-events-none absolute bottom-2 left-2 rounded bg-black/40 px-2 py-1 font-mono text-[10px] text-[#9aa0ae]">
        {ready
          ? `${ready.stats.triangles} tris | ${ready.stats.vertexCount} verts | ${ready.stats.floats} floats`
          : "loading mesh…"}
      </div>
    </>
  );
}

export function QRViewer3D({ objectId, sizeMM }: { objectId: string; sizeMM: number }) {
  return (
    <div className="relative h-full w-full">
      <QRMesh objectId={objectId} sizeMM={sizeMM} />
    </div>
  );
}