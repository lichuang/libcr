#ifndef __ID_MAP_H__
#define __ID_MAP_H__

/* max pid, equal to 2^15=32768 */
#define ID_MAX_DEFAULT 0x8000

class IdMap {
public:
  IdMap();
  ~IdMap();

  int   Allocate();
  void  Free(int id);

private:
  int nr_free_;
  int last_id_;
  char page_[ID_MAX_DEFAULT];
};

#endif  /* __ID_MAP_H__ */
