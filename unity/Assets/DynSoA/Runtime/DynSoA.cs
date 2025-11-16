// DynSoA Runtime SDK


using System;
using System.Runtime.InteropServices;

namespace DynSoA
{
    public enum Device : byte { CPU = 0, GPU = 1 }
    public enum ScalarType : byte { F32=0, I32=1, U32=2, F64=3, I64=4 }

    [StructLayout(LayoutKind.Sequential)]
    public struct Config { public Device device; public int aosoa_tile, matrix_block, max_retile_us; [MarshalAs(UnmanagedType.I1)] public bool scheduler_enabled; }

    [StructLayout(LayoutKind.Sequential)] public struct Field { public IntPtr name; public ScalarType type; }
    [StructLayout(LayoutKind.Sequential)] public struct Component { public IntPtr name; public IntPtr fields; public int field_count; }
    [StructLayout(LayoutKind.Sequential)] public struct KernelCtx { public float dt; public int tile; }
    [StructLayout(LayoutKind.Sequential)] public struct MatrixBlock { public IntPtr data; public int rows, cols, leading_dim; public UIntPtr bytes, offset; }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Sample {
        public IntPtr kernel; public ulong view;
        public float warp_eff, branch_div, mem_coalesce, l2_miss_rate;
        public uint time_us, p95_tile_us, p99_tile_us;
    }

    public static class Native
    {
        const string LIB = "dynsoa";

        [DllImport(LIB)] public static extern void dynsoa_init(ref Config cfg);
        [DllImport(LIB)] public static extern void dynsoa_shutdown();

        [DllImport(LIB)] public static extern void dynsoa_define_component(ref Component c);
        [DllImport(LIB, CharSet=CharSet.Ansi)] public static extern ulong dynsoa_define_archetype(string name, string[] comps, int count);

        [DllImport(LIB)] public static extern IntPtr dynsoa_spawn(ulong arch, UIntPtr count, IntPtr init_fn);
        [DllImport(LIB)] public static extern ulong dynsoa_make_view(ulong arch);
        [DllImport(LIB)] public static extern UIntPtr dynsoa_view_len(ulong view);
        [DllImport(LIB, CharSet=CharSet.Ansi)] public static extern IntPtr dynsoa_column(ulong view, string path);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void KernelFn(ulong view, ref KernelCtx ctx);

        [DllImport(LIB)] public static extern void dynsoa_begin_frame();
        [DllImport(LIB, CharSet=CharSet.Ansi)] public static extern void dynsoa_run_kernel(string name, KernelFn fn, ulong view, ref KernelCtx ctx);
        [DllImport(LIB)] public static extern void dynsoa_end_frame();

        [DllImport(LIB)] public static extern int dynsoa_retile_aosoa_plan_apply(ulong view, int tile);
        [DllImport(LIB)] public static extern int dynsoa_retile_to_soa(ulong view);

        [DllImport(LIB)] public static extern IntPtr dynsoa_acquire_matrix_block(ulong view, string[] comps, int k, int rows, UIntPtr offset, out MatrixBlock outBlock);
        [DllImport(LIB)] public static extern void dynsoa_release_matrix_block(ulong view, ref MatrixBlock block, int write_back);

        [DllImport(LIB, CharSet=CharSet.Ansi)] public static extern void dynsoa_set_policy(string jsonOrEmpty);

        [DllImport(LIB, CharSet=CharSet.Ansi)] public static extern void dynsoa_metrics_enable_csv(string path);
        [DllImport(LIB)] public static extern void dynsoa_emit_metric(ref Sample s);
    }

    public static class DynSoA
    {
        public static void Init(Config cfg) => Native.dynsoa_init(ref cfg);
        public static void Shutdown() => Native.dynsoa_shutdown();

        public static void DefineComponent(string name, (string field, ScalarType type)[] fields) {
            var f = new Field[fields.Length];
            IntPtr fPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Field>() * fields.Length);
            for (int i = 0; i < fields.Length; i++) {
                f[i].name = Marshal.StringToHGlobalAnsi(fields[i].field);
                f[i].type = fields[i].type;
                Marshal.StructureToPtr(f[i], fPtr + i * Marshal.SizeOf<Field>(), false);
            }
            var comp = new Component {
                name = Marshal.StringToHGlobalAnsi(name),
                fields = fPtr,
                field_count = fields.Length
            };
            Native.dynsoa_define_component(ref comp);
            Marshal.FreeHGlobal(comp.name);
            for (int i = 0; i < fields.Length; i++) Marshal.FreeHGlobal(f[i].name);
            Marshal.FreeHGlobal(fPtr);
        }

        public static ulong DefineArchetype(string name, string[] components)
            => Native.dynsoa_define_archetype(name, components, components.Length);

        public static void Spawn(ulong arch, ulong count)
            => Native.dynsoa_spawn(arch, (UIntPtr)count, IntPtr.Zero);

        public static ulong MakeView(ulong arch) => Native.dynsoa_make_view(arch);
        public static int ViewLen(ulong view) => (int)Native.dynsoa_view_len(view);

        public static unsafe Span<float> ColF32(ulong view, string path, int len) {
            IntPtr ptr = Native.dynsoa_column(view, path);
            return new Span<float>((void*)ptr, len);
        }

        public static void BeginFrame() => Native.dynsoa_begin_frame();
        public static void EndFrame() => Native.dynsoa_end_frame();

        public static void RunKernel(string name, Native.KernelFn fn, ulong view, KernelCtx ctx)
            => Native.dynsoa_run_kernel(name, fn, view, ref ctx);

        public static bool RetileAoSoA(ulong view, int tile) => Native.dynsoa_retile_aosoa_plan_apply(view, tile) != 0;
        public static bool RetileToSoA(ulong view) => Native.dynsoa_retile_to_soa(view) != 0;

        public static MatrixBlock AcquireMatrixBlock(ulong view, string[] comps, int rows, ulong offset = 0) {
            Native.dynsoa_acquire_matrix_block(view, comps, comps.Length, rows, (UIntPtr)offset, out MatrixBlock mb);
            return mb;
        }
        public static void ReleaseMatrixBlock(ulong view, ref MatrixBlock mb, bool writeBack=false)
            => Native.dynsoa_release_matrix_block(view, ref mb, writeBack?1:0);

        public static void SetPolicy(string jsonOrEmpty) => Native.dynsoa_set_policy(jsonOrEmpty);
        public static void EnableCSV(string path) => Native.dynsoa_metrics_enable_csv(path);
        public static void EmitMetric(Native.Sample s) => Native.dynsoa_emit_metric(ref s);
    }
}
