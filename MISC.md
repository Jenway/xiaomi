## 番外篇：被 Git LFS + Git Subtree 气死

所以我现在的需求是

```bash
❯ ls
lab01  lab03  lab05  lab07  lab09  lab11
lab02  lab04  lab06  lab08  lab10  lab12
```

每个目录都是一个独立的 Git 仓库。

现在想整合为一个 `monorepo`，并保留各自的提交记录，期望的 `commit-log` 大致如下：

```diff
--- Merge lab03 history
--- commit of lab03 xxx
--- commit of lab03 xxx
--- Merge lab02 history
--- commit of lab02 xxx
--- commit of lab01 xxx
```

### Try git subtree

那么首先被推荐的自然是 `git subtree`，它的用法类似：

```bash
git subtree add --prefix <path> <repo> <branch>
```

那么很容易让 LLM 写个脚本 `./import.sh`

很好，结束了，结果很完美——如果你没有同时用 Git LFS 的话

> 什么，你也和我一样同时用了 Git LFS 和 git subtree？那你也要和我一样受折磨了

用了 Git LFS 的话，那么大概率连 `subtree` 命令都会失败，即使成功，往别处 push 的时候 Git 也会因为找不到 LFS 指针指向的文件而罢工

____

> 那我们先不要 LFS 文件了，都删掉怎么样？

额，NO，实际上直接把 LFS 文件都删掉也不行，因为历史里还会有 LFS pointers ，github 这种情况下是直接不允许你上传的

### STFW

搜索 `git lfs subtree` 关键词，你大概率会看到这些从 2015 年就开摆的问题：

- [git lfs not working with git subtree ? #854](https://github.com/git-lfs/git-lfs/issues/854)
- [Git subtree pull is not working with git LFS  #1948](https://github.com/git-lfs/git-lfs/issues/1948)

维护者是这样表示的：

> My previous comment stands:
>
> > It may happen to work (or not), but support for it isn't something we have planned. Of course, someone is free to pick it up if they'd like to implement it.
>
> As far as I'm aware, nobody on the core team uses subtrees (I use submodules instead), so nobody here has the expertise to implement this or make it work without seeing an external patch. I don't expect that will change anytime soon.
>
> If this functionality ever appears in a public release, it will be in the changelog.

### solution

我们采用一种暴力又实用的方法：删除历史中的 LFS pointer 文件，重写历史后再把这些文件手动加回来。

解决方案比较粗暴，那就是用 `git-filter-repo` 删除历史中的 LFS pointer 文件，重写历史后再把这些文件手动加回来。

**重写之前一定要备份，最粗暴的方法是把仓库复制一份**

首先自然把这些文件都找出来：

``` bash
git grep -I 'version https://git-lfs.github.com/spec' $(git rev-list --all) |  awk -F ':' '{print "--path "$2}' | sort | uniq > paths.txt
```

下载一下 `git-filter-repo`

``` bash
curl -O https://raw.githubusercontent.com/newren/git-filter-repo/main/git-filter-repo
chmod +x git-filter-repo
```

然后重写历史

``` bash
./git-filter-repo $(cat paths.txt) --invert-paths --force
```

这时候那些 LFS 文件就都不见了，从我们的备份文件里复制回去

```bash
rsync -av --progress --exclude='.git' --checksum backup/ mono/
```

由于 `.gitattributes` 还都保留着，所以直接 add 即可

### 清理下大文件

也可以趁着这个功夫来清理一下仓库中的大文件为 LFS，现在 LLM 总算能起点用处了

接下来的文本为 LLM 生成

______

``` bash
find . -type f -size +4M -print0 | xargs -0 du -h
```

例如看到以下大文件：


```bash
10M     ./lab12/androidplayer/app/src/main/jniLibs/x86_64/libffmpeg-jw.so
6.0M    ./lab11/part1/app/release/app-release.apk
```

备份这些大文件

```bash
mkdir -p temp_large_files_backup
cp --parents lab12/androidplayer/app/src/main/jniLibs/x86_64/libffmpeg-jw.so temp_large_files_backup/
cp --parents lab11/part1/app/release/app-release.apk temp_large_files_backup/

# 确认备份是否成功
ls -lhR temp_large_files_backup/
```

清除历史记录中的这些文件

```bash
./git-filter-repo \
  --path-glob 'lab12/androidplayer/app/src/main/jniLibs/x86_64/libffmpeg-jw.so' \
  --path-glob 'lab11/part1/app/release/app-release.apk' \
  --invert-paths --force
```

或者用文件列表：

```bash
echo '*.so' >> delete_patterns.txt
git filter-repo --paths-from-file delete_patterns.txt --invert-paths --force
```

恢复文件回工作区并重新提交

```bash
cp -r temp_large_files_backup/* .
rm -r temp_large_files_backup/
git add .
git commit -m "Re-add cleaned large files"
```

## 番外篇2：删除 Git Subree 以及它带来的历史

要删除很简单：

```bash
git subtree split --prefix <prefix>
```

但是如果你在 `subtree add` 后，（像我一样）又做了几个 commit，那完蛋了，你会发现 commit 历史记录赖着不走了，就算用 `git-filter-repo` 重写历史也没用，具体原因有个 issue 里有解释：

[Removing subtrees? #28](https://github.com/newren/git-filter-repo/issues/28)

> filter-repo by default only removes commits if they become empty; are there other changes that were made as part of those commits that are outside the paths you are removing? You may want to run git log --name-status ... to get a look at some of the commits in question and see which files they still modify. (I'll assume for now that we're not dealing with commits that started empty.) Of course, if you have a complex enough series of merges, then merge commits may need to be retained just to preserve the commit topology. So git log --graph --name-status ... might be handy.

最后没有办法了，好在我的 commit 积压的不多，只有之前的恢复 LFS 的 commit，心一横 reset 了

实在被折腾怕了，我是再也不敢搞 monorepo 了

