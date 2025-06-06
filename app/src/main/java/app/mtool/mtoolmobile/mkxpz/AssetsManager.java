package app.mtool.mtoolmobile.mkxpz;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.AsyncTask;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Assets 管理工具类
 * 用于复制 assets 到指定路径，支持进度回调
 */
public class AssetsManager {
    private static final String TAG = "AssetsManager";

    /**
     * 进度回调接口
     */
    public interface CopyProgressCallback {
        /**
         * 开始复制前调用
         * @param totalFiles 总文件数
         */
        void onStart(int totalFiles);

        /**
         * 复制进度更新时调用
         * @param currentFile 当前文件名
         * @param progress 进度 (0-100)
         * @param currentCount 当前处理的文件索引
         * @param totalFiles 总文件数
         */
        void onProgress(String currentFile, int progress, int currentCount, int totalFiles);

        /**
         * 复制完成时调用
         * @param success 是否成功
         */
        void onFinish(boolean success);
    }

    /**
     * 复制整个 assets 目录到指定路径
     * @param context 上下文
     * @param srcDir assets 中的源目录路径 (例如 "game" 或 "" 表示根目录)
     * @param destDir 目标路径
     * @param callback 进度回调
     * @return 是否复制成功
     */
    public static boolean copyAssets(Context context, String srcDir, String destDir, CopyProgressCallback callback) {
        new CopyAssetsTask(context, srcDir, destDir, callback).execute();
        return true;
    }

    /**
     * 复制整个 assets 目录到指定路径 (同步方法)
     * @param context 上下文
     * @param srcDir assets 中的源目录路径 (例如 "game" 或 "" 表示根目录)
     * @param destDir 目标路径
     * @param callback 进度回调
     * @return 是否复制成功
     */
    public static boolean copyAssetsSync(Context context, String srcDir, String destDir, CopyProgressCallback callback) {
        AssetManager assetManager = context.getAssets();
        List<String> files = new ArrayList<>();

        try {
            // 先递归获取所有文件列表
            listAssetFiles(assetManager, srcDir, "", files);

            if (callback != null) {
                callback.onStart(files.size());
            }

            // 创建目标根目录
            File destDirectory = new File(destDir);
            if (!destDirectory.exists()) {
                destDirectory.mkdirs();
            }

            // 逐个复制文件
            int count = 0;
            for (String fileName : files) {
                count++;
                if (callback != null) {
                    callback.onProgress(fileName, count * 100 / files.size(), count, files.size());
                }

                String sourceFileName = srcDir.isEmpty() ? fileName : (srcDir + "/" + fileName);
                File destFile = new File(destDir, fileName);

                // 确保父目录存在
                File parent = destFile.getParentFile();
                if (parent != null && !parent.exists()) {
                    parent.mkdirs();
                }

                // 复制文件
                InputStream in = null;
                OutputStream out = null;

                try {
                    in = assetManager.open(sourceFileName);
                    out = new FileOutputStream(destFile);
                    copyFile(in, out);
                } catch (IOException e) {
                    Log.e(TAG, "Failed to copy asset file: " + fileName, e);
                    if (callback != null) {
                        callback.onFinish(false);
                    }
                    return false;
                } finally {
                    if (in != null) {
                        try {
                            in.close();
                        } catch (IOException e) {
                            // 忽略
                        }
                    }
                    if (out != null) {
                        try {
                            out.close();
                        } catch (IOException e) {
                            // 忽略
                        }
                    }
                }
            }

            if (callback != null) {
                callback.onFinish(true);
            }
            return true;

        } catch (IOException e) {
            Log.e(TAG, "Failed to copy assets", e);
            if (callback != null) {
                callback.onFinish(false);
            }
            return false;
        }
    }

    /**
     * 递归列出指定 assets 目录下的所有文件
     * @param assetManager AssetManager 实例
     * @param dirPath assets 中的目录路径
     * @param currentPath 当前路径 (用于递归)
     * @param fileList 文件列表 (用于存储结果)
     * @throws IOException 如果读取文件列表失败
     */
    private static void listAssetFiles(AssetManager assetManager, String dirPath, String currentPath,
                                      List<String> fileList) throws IOException {
        String fullPath = dirPath.isEmpty() ? currentPath : (dirPath + "/" + currentPath);
        String[] list = assetManager.list(fullPath);

        if (list != null && list.length > 0) {
            // 是目录，递归处理
            for (String file : list) {
                String newPath = currentPath.isEmpty() ? file : (currentPath + "/" + file);
                if (assetManager.list(dirPath.isEmpty() ? newPath : (dirPath + "/" + newPath)).length == 0) {
                    // 是文件
                    fileList.add(newPath);
                } else {
                    // 是目录
                    listAssetFiles(assetManager, dirPath, newPath, fileList);
                }
            }
        } else if (!currentPath.isEmpty()) {
            // 是文件
            fileList.add(currentPath);
        }
    }

    /**
     * 复制单个文件
     * @param in 输入流
     * @param out 输出流
     * @throws IOException 如果复制失败
     */
    private static void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[8192];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    /**
     * 复制单个 assets 文件到指定路径
     * @param context 上下文
     * @param assetFilePath assets 中的文件路径
     * @param destFilePath 目标文件路径
     * @return 是否复制成功
     */
    public static boolean copyAssetFile(Context context, String assetFilePath, String destFilePath) {
        try {
            AssetManager assetManager = context.getAssets();
            InputStream in = assetManager.open(assetFilePath);

            File destFile = new File(destFilePath);
            File parent = destFile.getParentFile();
            if (parent != null && !parent.exists()) {
                parent.mkdirs();
            }

            OutputStream out = new FileOutputStream(destFile);
            copyFile(in, out);
            in.close();
            out.close();
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed to copy asset file: " + assetFilePath, e);
            return false;
        }
    }

    /**
     * 异步复制任务
     */
    private static class CopyAssetsTask extends AsyncTask<Void, Object, Boolean> {
        private final Context context;
        private final String srcDir;
        private final String destDir;
        private final CopyProgressCallback callback;
        private List<String> files;

        public CopyAssetsTask(Context context, String srcDir, String destDir, CopyProgressCallback callback) {
            this.context = context;
            this.srcDir = srcDir;
            this.destDir = destDir;
            this.callback = callback;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
        }

        @Override
        protected Boolean doInBackground(Void... params) {
            AssetManager assetManager = context.getAssets();
            files = new ArrayList<>();

            try {
                // 先递归获取所有文件列表
                listAssetFiles(assetManager, srcDir, "", files);

                publishProgress(0, 0, files.size()); // 发布总文件数

                // 创建目标根目录
                File destDirectory = new File(destDir);
                if (!destDirectory.exists()) {
                    destDirectory.mkdirs();
                }

                // 逐个复制文件
                int count = 0;
                for (String fileName : files) {
                    count++;
                    publishProgress(1, fileName, count, files.size());

                    String sourceFileName = srcDir.isEmpty() ? fileName : (srcDir + "/" + fileName);
                    File destFile = new File(destDir, fileName);

                    // 确保父目录存在
                    File parent = destFile.getParentFile();
                    if (parent != null && !parent.exists()) {
                        parent.mkdirs();
                    }

                    // 复制文件
                    InputStream in = null;
                    OutputStream out = null;

                    try {
                        in = assetManager.open(sourceFileName);
                        out = new FileOutputStream(destFile);
                        copyFile(in, out);
                    } catch (IOException e) {
                        Log.e(TAG, "Failed to copy asset file: " + fileName, e);
                        return false;
                    } finally {
                        if (in != null) {
                            try {
                                in.close();
                            } catch (IOException e) {
                                // 忽略
                            }
                        }
                        if (out != null) {
                            try {
                                out.close();
                            } catch (IOException e) {
                                // 忽略
                            }
                        }
                    }
                }

                return true;

            } catch (IOException e) {
                Log.e(TAG, "Failed to copy assets", e);
                return false;
            }
        }

        @Override
        protected void onProgressUpdate(Object... values) {
            if (callback == null) {
                return;
            }

            int type = (int) values[0];

            if (type == 0) {
                // onStart
                callback.onStart((int) values[2]);
            } else if (type == 1) {
                // onProgress
                String fileName = (String) values[1];
                int count = (int) values[2];
                int total = (int) values[3];
                int progress = count * 100 / total;
                callback.onProgress(fileName, progress, count, total);
            }
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (callback != null) {
                callback.onFinish(result);
            }
        }
    }
}
