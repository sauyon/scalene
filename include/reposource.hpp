#ifndef REPOSOURCE_HPP
#define REPOSOURCE_HPP

#include "common.hpp"
#include "repo.hpp"

template <int Size>
class RepoSource {
private:
  
  enum { MAX_HEAP_SIZE = 3UL * 1024 * 1024 * 1024 }; // 3GB

  const char * _bufferStart;
  char * _buf;
  size_t _sz;

  constexpr auto align (void * ptr) {
    const auto alignedPtr = (void *) (((uintptr_t) ptr + Size - 1) & ~(Size - 1));
    return alignedPtr;
  }
  
public:

  RepoSource()
    : _bufferStart (reinterpret_cast<char *>(MmapWrapper::map(MAX_HEAP_SIZE))),
      // Below, align the buffer and subtract the part removed by aligning it.
      _buf(reinterpret_cast<char *>(align((void *) _bufferStart))),
      _sz (MAX_HEAP_SIZE - ((uintptr_t) align((void *) _bufferStart) - (uintptr_t) _bufferStart))
  {
    static int counter = 0;
    counter++;
    // Sanity check.
    if (counter > 1) {
      abort();
    }
  }

  constexpr auto getHeapSize() {
    return MAX_HEAP_SIZE;
  }
  
  inline const char * getBufferStart() {
    return _bufferStart;
  }
  
  inline bool isValid() const {
#if defined(NDEBUG)
    return true;
#endif
    return true;
  }
  
  Repo<Size> * get(size_t sz) {
    // tprintf::tprintf("repo: @,  (buf = @), @ get @\n", this, _buf, Size, sz);
    assert(isValid());
    auto index = getIndex(sz);
    Repo<Size> * repo = nullptr;
    if (unlikely(getSource(index) == nullptr)) {
      // Nothing in this size. Check the empty list.
      if (getSource(NUM_REPOS) == nullptr) {
	// No empties. Allocate a new one.
	if (likely(sz < _sz)) {
	  auto buf = _buf;
	  _buf += Size;
	  _sz -= Size;
	  repo = new (buf) Repo<Size>(sz);
	  assert(repo != nullptr);
	  repo->setNext(nullptr); // FIXME? presumably redundant.
	  assert (repo->getState() == RepoHeader<Size>::RepoState::Unattached);
	  assert(isValid());
	  return repo;
	} else {
	  tprintf::tprintf("Scalene: Memory exhausted: sz = @\n", sz);
	  assert(isValid());
	  return nullptr;
	}
      }
    }
    // If we get here, either there's a repo with the desired size or there's an empty available.
    repo = getSource(index);
    if (!repo || (!repo->isEmpty() && getSource(NUM_REPOS) != nullptr)) {
      // Reformat an empty repo.
      index = NUM_REPOS;
      repo = getSource(index);
      assert(repo->isEmpty());
      repo = new (repo) Repo<Size>(sz);
      assert(repo->getObjectSize() == sz);
    }
    auto oldState = repo->setState(RepoHeader<Size>::RepoState::Unattached);
    // tprintf::tprintf("GET (@) popping @.\n", sz, repo);
    getSource(index) = getSource(index)->getNext();
    repo->setNext(nullptr);
    if (getSource(index) != nullptr) {
      assert(getSource(index)->isValid());
    }
    assert(repo->isValid());
    assert(isValid());
    assert(repo->getNext() == nullptr);
    return repo;
  }

  void put(Repo<Size> * repo) {
    assert(isValid());
    assert(repo != nullptr);
    assert(repo->isValid());
    if (repo->getState() == RepoHeader<Size>::RepoState::RepoSource) {
      // This should never happen.
      // Fail gracefully.
      return;
    }
    auto oldState = repo->setState(RepoHeader<Size>::RepoState::RepoSource);
    assert(oldState != RepoHeader<Size>::RepoState::RepoSource);
    assert(repo->getNext() == nullptr);
    auto index = getIndex(repo->getSize(nullptr));
    if (repo->isEmpty()) {
      // Put empty repos on the last array.
      // Temporarily disabled.
      ///      index = NUM_REPOS;
    }
    repo->setNext(getSource(index));
    getSource(index) = repo;
    assert(isValid());
  }
  
private:

  RepoSource(RepoSource&);
  RepoSource& operator=(const RepoSource&);
  
  // TBD: unify with RepoMan declarations.
  
  enum { MULTIPLE = 16 };
  enum { MAX_SIZE = 512 };
  enum { NUM_REPOS = Size / MULTIPLE };
  
  static ATTRIBUTE_ALWAYS_INLINE constexpr inline int getIndex(size_t sz) {
    return sz / MULTIPLE - 1;
  }
  
  static Repo<Size> *& getSource(int index) {
    assert(index >= 0);
    assert(index <= NUM_REPOS);
    static Repo<Size> * repos[NUM_REPOS + 1] { nullptr }; // One extra at the end for empty repos.
    return repos[index];
  }
  
};

#endif
