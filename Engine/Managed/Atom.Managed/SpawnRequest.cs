namespace Atom;
/// <summary>延迟创建或附加预制体的请求；在回调安全点后轮询，新增脚本下一帧开始执行。</summary>
public sealed class SpawnRequest
{
    private readonly GameObject owner;
    private readonly ulong request;
    private bool completed, failed;
    private GameObject? result;
    internal SpawnRequest(GameObject owner, ulong request) { this.owner = owner; this.request = request; }
    private unsafe void Poll()
    {
        if (completed) return;
        if (owner.Api.Epoch(owner.Context) != owner.Generation) { completed = failed = true; return; }
        uint entity = 0; int status = owner.Api.Result(owner.Context, request, &entity);
        if (status == 0) return;
        completed = true; failed = status < 0;
        if (!failed) result = owner.Wrap(entity);
    }
    /// <summary>请求已经成功或失败时为真；读取此属性会轮询引擎结果。</summary>
    public bool IsCompleted { get { Poll(); return completed; } }
    /// <summary>请求执行失败或所属世界结束时为真；仍在等待时为假。</summary>
    public bool Failed { get { Poll(); return failed; } }
    /// <summary>成功后的对象引用；等待时为 null，失败时抛出异常；同一请求缓存结果。</summary>
    public GameObject? Result { get { Poll(); if (failed) throw new InvalidOperationException("Spawn failed or its World ended; see engine log"); return result; } }
}
