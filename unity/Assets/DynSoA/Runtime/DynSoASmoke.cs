// DynSoA Runtime SDK
// (C) 2025 Sungmin "Jason" Jun
//
// Covered under:
//  - U.S. Patent Application No. 19/303,020
//  - U.S. Provisional No. 63/775,990
//  - CIP: Systems and Methods for Adaptive Optimization and Coordination of Data Layout and Execution
//
// Licensed under the Mozilla Public License 2.0 (MPL 2.0).
// Commercial use requires a separate commercial license.
// Unauthorized commercial use may infringe one or more patents.


using UnityEngine;
using DynSoA;

public class DynSoASmoke : MonoBehaviour
{
    ulong arch, view;

    void Start()
    {
        var cfg = new Config { device = Device.CPU, aosoa_tile = 128, matrix_block = 1024, max_retile_us = 500, scheduler_enabled = true };
        DynSoA.DynSoA.Init(cfg);

        DynSoA.DynSoA.DefineComponent("Position", new (string, ScalarType)[]{
            ("Position.x",ScalarType.F32), ("Position.y",ScalarType.F32), ("Position.z",ScalarType.F32)});
        DynSoA.DynSoA.DefineComponent("Velocity", new (string, ScalarType)[]{
            ("Velocity.vx",ScalarType.F32), ("Velocity.vy",ScalarType.F32), ("Velocity.vz",ScalarType.F32)});

        arch = DynSoA.DynSoA.DefineArchetype("RigidBody", new[]{ "Position", "Velocity" });
        DynSoA.DynSoA.Spawn(arch, 200_000);
        view = DynSoA.DynSoA.MakeView(arch);

        DynSoA.DynSoA.EnableCSV(Application.persistentDataPath + "/dynsoa_bench.csv");
        DynSoA.DynSoA.SetPolicy("{}");
    }

    static void PhysicsStep(ulong v, ref KernelCtx ctx)
    {
        int n = DynSoA.DynSoA.ViewLen(v);
        var px = DynSoA.DynSoA.ColF32(v, "Position.x", n);
        var vx = DynSoA.DynSoA.ColF32(v, "Velocity.vx", n);
        for (int i = 0; i < n; ++i) px[i] += vx[i] * ctx.dt;
    }

    void Update()
    {
        var ctx = new KernelCtx { dt = Time.deltaTime, tile = 131072 };
        DynSoA.DynSoA.BeginFrame();
        DynSoA.Native.KernelFn fn = PhysicsStep;
        DynSoA.DynSoA.RunKernel("physics_step", fn, view, ctx);
        DynSoA.DynSoA.EndFrame();

        if (Input.GetKeyDown(KeyCode.R)) Debug.Log("Retile AoSoA: " + DynSoA.DynSoA.RetileAoSoA(view, 128));
        if (Input.GetKeyDown(KeyCode.S)) Debug.Log("Retile SoA: " + DynSoA.DynSoA.RetileToSoA(view));
    }

    void OnDestroy() => DynSoA.DynSoA.Shutdown();
}
