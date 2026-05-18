# Warp Timeline Viewer

Generate a timeline:

```bash
./build-ninja/simt_gpu --timeline outputs/gpu_warp_timeline.csv examples/vector_add.gpuasm
```

Open `viewer/gpu_warp_timeline_viewer.html` and select the CSV. The viewer displays cycles across the top and SM/warp rows down the side. It filters by SM, warp, event type, and stall reason.

CSV columns:

```text
cycle,sm,cta,warp,pc,instruction,event,active_mask,eligible,selected,stall_reason,memory_transaction_count,cache_result,divergence_depth
```
