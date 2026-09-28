local M = {}

function M.read(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local content = f:read("a")
  f:close()
  return content
end

-- Windows os.rename 은 대상이 있으면 실패하므로 삭제 후 rename 한다.
-- 그 사이 읽는 쪽은 파일이 잠깐 없을 수 있으며, 읽는 쪽이 다음 폴링에서 재시도한다.
function M.writeAtomic(path, content)
  local tmp = path .. ".tmp"
  local f, err = io.open(tmp, "wb")
  if not f then return false, err end
  f:write(content)
  f:close()
  os.remove(path)
  local ok, rerr = os.rename(tmp, path)
  if not ok then
    os.remove(tmp)
    return false, rerr
  end
  return true
end

return M
