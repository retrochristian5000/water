#include "ddk/win95rtm/network/dma_policy.h"
#include <stdio.h>

#define CHECK(p) do {++checks; if (!(p)) {fprintf(stderr,"failed at %d: %s\n",__LINE__,#p);return 1;}}while(0)
static unsigned char memory[8192];
static int allocated, released, sync_to, sync_from, fail_alloc, fail_sync_to, fail_sync_from;
static uint32_t map_low=0x4000, map_high=0;
static uint32_t last_free_low,last_free_high,last_free_length;
static int last_free_cached;
static void *last_sync_addr;
static uint32_t last_sync_len;
static int last_sync_direction;
static int allocation(void *owner,uint32_t n,int cached,void **va,uint32_t *lo,uint32_t *hi)
{
    if (owner!=(void *)memory || n<1 || cached) return 0;
    if (fail_alloc) return 0;
    ++allocated;
    *va=memory;*lo=map_low;*hi=map_high;
    return 1;
}
static void release(void *owner,uint32_t n,int cached,void *va,uint32_t lo,uint32_t hi)
{
    if(owner!=(void *)memory || va!=memory) { fprintf(stderr,"invalid release tuple\n");return; }
    ++released;last_free_length=n;last_free_low=lo;last_free_high=hi;last_free_cached=cached;
}
static int to_device(void *owner,void *addr,uint32_t length,int from_device)
{
    if(owner!=memory) return 0;
    ++sync_to;last_sync_addr=addr;last_sync_len=length;last_sync_direction=from_device;
    return !fail_sync_to;
}
static int to_cpu(void *owner,void *addr,uint32_t length,int from_device)
{
    if(owner!=memory) return 0;
    ++sync_from;last_sync_addr=addr;last_sync_len=length;last_sync_direction=from_device;
    return !fail_sync_from;
}
static const struct whp_ndis31_dma_ops ops = {allocation,release,to_device,to_cpu};

int main(void)
{
    struct whp_ndis31_dma d={0},missing={0};
    struct whp_ndis31_dma_ticket a,b,c,stale;
    struct whp_ndis31_dma_ops incomplete=ops;
    uint32_t bus=0;int checks=0;unsigned i;
    CHECK(whp_ndis31_dma_construct(&d)==WHP_NDIS31_DMA_OK);
    CHECK(whp_ndis31_dma_begin(&d,0,64,0,&a,&bus)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,0,256,0)==WHP_NDIS31_DMA_BAD_ARG);
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,8192*1024,256,0)==WHP_NDIS31_DMA_BAD_ARG);
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,3,0)==WHP_NDIS31_DMA_BAD_ARG);
    incomplete.sync_for_cpu=0;
    CHECK(whp_ndis31_dma_allocate(&d,&incomplete,memory,4096,256,0)==WHP_NDIS31_DMA_BAD_ARG);
    CHECK(allocated==0);
    fail_alloc=1;
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,256,0)==WHP_NDIS31_DMA_ALLOC_FAILED);
    CHECK(d.state==WHP_NDIS31_DMA_EMPTY && !d.base);
    fail_alloc=0;
    map_high=1;
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,256,0)==WHP_NDIS31_DMA_BAD_MAPPING);
    CHECK(allocated==1 && released==1 && last_free_high==1);
    map_high=0;map_low=0xfffff100;
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,256,0)==WHP_NDIS31_DMA_BAD_MAPPING);
    CHECK(allocated==2 && released==2);
    map_low=0x4001;
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,256,0)==WHP_NDIS31_DMA_BAD_MAPPING);
    CHECK(allocated==3 && released==3);
    map_low=0x4000;
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,256,0)==WHP_NDIS31_DMA_OK);
    CHECK(allocated==4 && released==3 && d.length==4096 && d.low==0x4000);
    CHECK(whp_ndis31_dma_construct(&d)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,256,0)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_begin(&d,4096,1,0,&a,&bus)==WHP_NDIS31_DMA_BAD_ARG);
    CHECK(whp_ndis31_dma_begin(&d,4095,2,0,&a,&bus)==WHP_NDIS31_DMA_BAD_ARG);
    CHECK(whp_ndis31_dma_begin(&d,0,0,0,&a,&bus)==WHP_NDIS31_DMA_BAD_ARG);
    CHECK(whp_ndis31_dma_begin(&d,0,64,2,&a,&bus)==WHP_NDIS31_DMA_BAD_ARG);
    fail_sync_to=1;
    CHECK(whp_ndis31_dma_begin(&d,0,64,0,&a,&bus)==WHP_NDIS31_DMA_SYNC_FAILED);
    CHECK(d.active==0);
    fail_sync_to=0;
    CHECK(whp_ndis31_dma_begin(&d,0,64,0,&a,&bus)==WHP_NDIS31_DMA_OK);
    CHECK(bus==0x4000 && a.serial!=0 && last_sync_addr==memory && last_sync_len==64 && last_sync_direction==0);
    CHECK(whp_ndis31_dma_begin(&d,63,32,1,&b,&bus)==WHP_NDIS31_DMA_BUSY);
    CHECK(whp_ndis31_dma_begin(&d,64,128,1,&b,&bus)==WHP_NDIS31_DMA_OK);
    CHECK(bus==0x4040 && b.serial!=a.serial && last_sync_direction==1);
    CHECK(whp_ndis31_dma_ack(&d,a)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_quiesce(&d)==WHP_NDIS31_DMA_OK);
    CHECK(whp_ndis31_dma_begin(&d,512,64,0,&c,&bus)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_release(&d)==WHP_NDIS31_DMA_BUSY);
    fail_sync_from=1;
    CHECK(whp_ndis31_dma_complete(&d,a)==WHP_NDIS31_DMA_SYNC_FAILED);
    CHECK(whp_ndis31_dma_ack(&d,a)==WHP_NDIS31_DMA_BAD_STATE);
    fail_sync_from=0;
    CHECK(whp_ndis31_dma_complete(&d,a)==WHP_NDIS31_DMA_OK);
    CHECK(last_sync_addr==memory && last_sync_len==64 && last_sync_direction==0);
    CHECK(whp_ndis31_dma_complete(&d,a)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_complete(&d,b)==WHP_NDIS31_DMA_OK);
    CHECK(last_sync_addr==memory+64 && last_sync_len==128 && last_sync_direction==1);
    CHECK(whp_ndis31_dma_ack(&d,b)==WHP_NDIS31_DMA_OK);
    CHECK(whp_ndis31_dma_ack(&d,b)==WHP_NDIS31_DMA_BAD_ARG);
    stale=a;
    CHECK(whp_ndis31_dma_ack(&d,a)==WHP_NDIS31_DMA_OK);
    CHECK(d.active==0);
    CHECK(whp_ndis31_dma_release(&d)==WHP_NDIS31_DMA_OK);
    CHECK(released==4 && last_free_length==4096 && last_free_low==0x4000 && last_free_cached==0);
    CHECK(d.state==WHP_NDIS31_DMA_EMPTY && d.base==0);
    CHECK(whp_ndis31_dma_construct(&d)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_release(&d)==WHP_NDIS31_DMA_BAD_STATE);
    CHECK(whp_ndis31_dma_allocate(&d,&ops,memory,4096,256,0)==WHP_NDIS31_DMA_OK);
    CHECK(whp_ndis31_dma_begin(&d,0,64,0,&c,&bus)==WHP_NDIS31_DMA_OK);
    CHECK(c.serial!=stale.serial);
    CHECK(whp_ndis31_dma_complete(&d,stale)==WHP_NDIS31_DMA_BAD_ARG);
    CHECK(whp_ndis31_dma_complete(&d,c)==WHP_NDIS31_DMA_OK);
    CHECK(whp_ndis31_dma_ack(&d,c)==WHP_NDIS31_DMA_OK);
    CHECK(whp_ndis31_dma_quiesce(&d)==WHP_NDIS31_DMA_OK);
    CHECK(whp_ndis31_dma_release(&d)==WHP_NDIS31_DMA_OK);
    CHECK(released==5);
    for(i=0;i<2;++i) CHECK(whp_ndis31_dma_construct(&missing)==WHP_NDIS31_DMA_OK);
    CHECK(missing.state==WHP_NDIS31_DMA_EMPTY);
    printf("WHP NDIS 3.1 portable DMA policy: %d checks passed\n",checks);
    return 0;
}
