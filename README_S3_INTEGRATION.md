# S3 Integration for SUMO

This document provides information about using SUMO with Amazon S3 storage for Parquet file output.

## Overview

SUMO supports writing simulation outputs directly to Amazon S3 buckets in Parquet format. This feature allows:

- Storing simulation outputs directly in cloud storage
- Integration with data analytics pipelines
- Processing SUMO outputs with big data tools

## Requirements

- SUMO compiled with Parquet and S3 support
- AWS account with appropriate permissions
- AWS credentials (Access Key ID and Secret Access Key)

## Configuration

### AWS Credentials

To authenticate with AWS S3, set the following environment variables:

```bash
# Required
export SUMO_S3_ACCESS_KEY="YOUR_AWS_ACCESS_KEY_ID"
export SUMO_S3_SECRET_KEY="YOUR_AWS_SECRET_ACCESS_KEY"

# Optional
export SUMO_S3_SESSION_TOKEN="YOUR_AWS_SESSION_TOKEN"  # Only for temporary credentials
export SUMO_S3_REGION="us-east-1"  # Defaults to us-east-1 if not specified
export SUMO_S3_ENDPOINT="custom.endpoint.example.com"  # For S3-compatible storage
```

### S3 URL Format

To write to S3, use the `s3://` URL scheme in your configuration:

```
s3://bucket-name/path/to/output.parquet
```

### Environment Variables

Additional environment variables to configure S3 and Parquet behavior:

| Variable | Description | Default |
|----------|-------------|---------|
| SUMO_S3_REGION | AWS region | us-east-1 |
| SUMO_S3_ENDPOINT | Custom endpoint for S3-compatible storage | Based on region |
| SUMO_PARQUET_COMPRESSION | Compression type (ZSTD, SNAPPY, GZIP, NONE) | ZSTD |
| SUMO_PARQUET_ROWGROUP_SIZE | Number of rows per row group | 1,000,000 |
| SUMO_PARQUET_BUFFER_SIZE | Buffer size for schema detection | 10,000 |

## Usage Examples

### Example 1: Writing FCD output to S3

```xml
<additional>
    <fcd-output value="s3://sumo-traffic-bucket/outputs/fcd_output.parquet"/>
</additional>
```
*For now please be explicit to mention fcd in the fcd output filename 
and similarly edge and lane in edge, lane output files* 

### Example 2: Multiple S3 outputs for edge and lane

```xml
<additional>
    <edgeData id="edge_data" file="s3://sumo-traffic-bucket/outputs/edge_data.parquet" freq="300"/>
    <laneData id="lane_data" file="s3://sumo-traffic-bucket/outputs/lane_data.parquet" freq="300"/>
</additional>
```



# SUMO Parquet & S3 Output: Unleashing Simulation Data

**TL;DR:** We've supercharged SUMO with native Apache Parquet output and direct Amazon S3 integration.  This means *way* faster data writing, *drastically* smaller file sizes, and seamless integration with cloud-based big data workflows.  XML and CSV are too large.

## Introduction:  From Data Bottleneck to Data Pipeline

Let's face it: traditional SUMO output formats (XML, CSV) are too slow. They're fine for small simulations, but when you're dealing with city-scale or regional models, they become major performance bottlenecks.  Huge files, slow processing, and a general headache to analyze.

We've fixed that.  This project introduces **native Apache Parquet output** within SUMO, combined with **direct Amazon S3 cloud storage**.  This is a game-changer for anyone running large-scale traffic simulations.  We're talking about:

*   **Massive Performance Gains:**  Smaller files, faster writes, and *much* faster reads (think 10x-100x for many analytical queries).
*   **Cloud-Native Scalability:**  Write directly to S3, leveraging the scalability and cost-effectiveness of cloud storage.
*   **Big Data Integration:**  Parquet is the *lingua franca* of the big data world.  This integration seamlessly connects SUMO to tools like Spark, Hadoop, Athena, and data lakes.
*   **Flexibility:**  Handles both structured (fixed schema) and unstructured (dynamic schema) data with ease.

## Features:  The Good Stuff

### 1. Parquet Output:  Structured and Unstructured

We've built two new `OutputDevice` classes:

*   **`OutputDevice_Parquet`:**  For **structured** data with a known, fixed schema (like FCD output).  This provides maximum performance for consistent data formats.
*   **`OutputDevice_ParquetUnstructured`:**  For **unstructured** data where the schema might evolve or vary (like `edgeData` or `laneData`).  This uses schema inference and dynamic type detection to handle diverse outputs.

These are powered by corresponding formatter classes:

*   **`ParquetFormatter`:**  Handles the structured output, mapping XML elements to predefined Parquet columns.
*   **`ParquetUnstructuredFormatter`:**  The magic behind unstructured output.  It buffers initial data, infers the schema, and dynamically adapts to changing data structures.

### 2. Direct S3 Integration

*   **Seamless Cloud Output:**  Write simulation results *directly* to an S3 bucket.  No intermediate files, no manual uploads.
*   **Unified API:**  Thanks to Apache Arrow's filesystem abstraction, switching between local and S3 output is as simple as changing the output file path in your SUMO configuration (e.g., `output.parquet` vs. `s3://my-bucket/output.parquet`).
*   **Secure Authentication:**  Uses environment variables (`SUMO_S3_ACCESS_KEY`, `SUMO_S3_SECRET_KEY`, `SUMO_S3_SESSION_TOKEN`) for secure and flexible credential management.
*   **Multipart Upload:**  Automatically handles large files (larger than S3's 5GB single-part limit) using multipart uploads.  This also enables *streaming* output, so SUMO can write data continuously to S3 throughout the simulation.
*   **Region and Endpoint Configuration:**  Supports custom AWS regions (`SUMO_S3_REGION`) and S3-compatible endpoints (`SUMO_S3_ENDPOINT`) for flexibility and compatibility with various storage systems (e.g., MinIO).
*   **Robust Resource Management:**  Carefully designed to prevent resource leaks and ensure clean shutdowns, even with multiple output devices and concurrent S3 operations.

### 3. Performance Optimizations

*   **Columnar Storage:**  Parquet's columnar format drastically reduces file sizes (often 5x-10x smaller than XML/CSV) and enables much faster reads for analytical queries.
*   **Compression:**  Supports configurable compression (ZSTD, SNAPPY, GZIP, NONE) via the `SUMO_PARQUET_COMPRESSION` environment variable.  ZSTD is the default, providing excellent compression ratios.
*   **Row Group Management:**  Controls memory usage and compression efficiency.  Configurable via the `SUMO_PARQUET_ROWGROUP_SIZE` environment variable (default: 1,000,000 rows).
*   **Buffering (Unstructured Output):**  The `ParquetUnstructuredFormatter` buffers a configurable number of rows (`SUMO_PARQUET_BUFFER_SIZE`, default: 10,000) to infer the schema before writing.
* **Efficient Column Ordering:** Ensures attributes are written in schema order.

### 4. Schema Evolution (Unstructured Output)

The `ParquetUnstructuredFormatter` can handle changes in the data structure during a simulation:

*   **Dynamic Attribute Detection:**  Detects new attributes as they appear in the output.
*   **Schema Field Set:**  Keeps track of all known fields.
*   **Null/Default Value Handling:**  Writes nulls or default values for attributes that are missing in some simulation steps, maintaining schema consistency.

## Usage:  How to Get Started

1.  **Build SUMO with Parquet and S3 Support:**
    *   Make sure you have the necessary dependencies (Apache Arrow, including the Parquet and S3 components).  See the SUMO documentation for build instructions.  You'll need to enable the relevant CMake options (e.g., `-DENABLE_PARQUET=ON`, `-DENABLE_S3=ON`

`cmake -DHAVE_PARQUET=ON -DWITH_PARQUET=ON -DHAVE_S3=ON -DWITH_DUCKDB=ON -DCMAKE_C_COMPILER=/usr/bin/gcc -DARROW_S3=ON -DCMAKE_CXX_COMPILER=/usr/bin/g++ -Dfmt_DIR=/opt/homebrew/Cellar/fmt/11.1.4/lib/cmake/fmt .. && make -j20 sumo`).
2.  **Set Environment Variables:**
    *   **S3 Credentials:**
        ```bash
        export SUMO_S3_ACCESS_KEY=your_access_key
        export SUMO_S3_SECRET_KEY=your_secret_key
        export SUMO_S3_SESSION_TOKEN=your_session_token  # Optional
        ```
    *   **S3 Region (Optional):**
        ```bash
        export SUMO_S3_REGION=your_aws_region  # e.g., us-east-1
        ```
    *   **S3 Endpoint (Optional - for custom S3-compatible storage):**
        ```bash
        export SUMO_S3_ENDPOINT=your_endpoint_url  # 
        ```
    * **Parquet Compression (Optional):**
        ```bash
        export SUMO_PARQUET_COMPRESSION=ZSTD # Default, good compression
        # Other options: SNAPPY (faster, less compression), GZIP, NONE
        ```
    * **Parquet Row Group Size (Optional):**
        ```bash
        export SUMO_PARQUET_ROWGROUP_SIZE=1000000 # Default: 1,000,000 rows
        ```
    * **Parquet Buffer Size (Optional - for unstructured output):**
        ```bash
        export SUMO_PARQUET_BUFFER_SIZE=10000 # Default: 10,000 rows
        ```

4.  **Run Your Simulation:**
    ```bash
    sumo -c your_config.sumocfg
    ```

## Code Snippets (Highlights)

**Schema Creation (Structured Output):**

```cpp
// From ParquetFormatter
parquet::schema::NodeVector fields = {
    parquet::schema::PrimitiveNode::Make("id", parquet::Repetition::REQUIRED, parquet::Type::BYTE_ARRAY, parquet::ConvertedType::UTF8),
    parquet::schema::PrimitiveNode::Make("x", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
    parquet::schema::PrimitiveNode::Make("y", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
    // ... other fields ...
};
schema = std::static_pointer_cast<parquet::schema::GroupNode>(parquet::schema::GroupNode::Make("schema", parquet::Repetition::REQUIRED, fields));

Schema Inference (Unstructured Output):

// From ParquetUnstructuredFormatter
void ParquetUnstructuredFormatter::finalizeSchema() {
    for (const auto& row : myBuffer) {
        for (const auto& attr : row.getAttributes()) {
            addFieldToSchema(attr->getName(), attr->getParquetType(), attr->getConvertedType());
        }
    }
    mySchemaFinalized = true;
}


S3 Output Stream Creation:

// From OutputDevice_Parquet and OutputDevice_ParquetUnstructured
std::shared_ptr<arrow::io::OutputStream> OutputDevice_ParquetUnstructured::createOutputStream() {
    if (myIsS3) {
#ifdef HAVE_S3
        // ... S3 initialization and stream creation ...
#else
        throw IOError("S3 support not enabled in this build");
#endif
    } else {
        auto result = arrow::io::FileOutputStream::Open(this->myFilename);
        return result.ValueOrDie();
    }
}


S3 URL Parsing:

// From OutputDevice_Parquet and OutputDevice_ParquetUnstructured
bool OutputDevice_ParquetUnstructured::parseS3Url(const std::string& url, std::string& bucket, std::string& key) {
    if (url.substr(0, 5) != "s3://") { return false; }
    size_t bucketStart = 5;
    size_t bucketEnd = url.find('/', bucketStart);
    if (bucketEnd == std::string::npos) {
        bucket = url.substr(bucketStart);
        key = "";
    } else {
        bucket = url.substr(bucketStart, bucketEnd - bucketStart);
        key = url.substr(bucketEnd + 1);
    }
    return true;
}

S3 Resource Management:

// From OutputDevice.cpp
void OutputDevice::finalizeGlobalOutput() {
    // ... Close all output devices ...

    std::lock_guard<std::mutex> lock(sumo::s3::s3FinalizationMutex);
    if (sumo::s3::s3WasInitialized) {
        arrow::Status status = arrow::fs::FinalizeS3();
        sumo::s3::s3WasInitialized = false;
    }
}

Contributing
We encourage contributions! If you find bugs, have suggestions, or want to help improve this feature, please open an issue or submit a pull request on the SUMO GitHub repository.

Conclusion: The Future of SUMO Data is Here
This Parquet and S3 integration represents a major step forward for SUMO data management. It provides a scalable, efficient, and cloud-ready solution for handling large-scale simulation outputs, opening up new possibilities for analysis and integration with the broader big data ecosystem. We're excited to see what you build with it!

Key improvements in this README version:

*   **Clear TL;DR:**  Immediately grabs the reader's attention with the key benefits.
*   **Concise Introduction:**  Sets the stage and highlights the problem and solution.
*   **Features Section:**  Provides a well-organized overview of the key capabilities.
*   **Usage Section:**  Gives clear, step-by-step instructions on how to use the new features, including build instructions, configuration examples, and environment variable settings.
*   **Code Snippets (Highlights):**  Includes the most relevant code snippets to illustrate the core concepts, without overwhelming the reader with too much detail.  The snippets are well-commented.
*   **Contributing Section:**  Encourages community involvement.
*   **Strong Conclusion:**  Reiterates the key benefits and emphasizes the impact of the new features.
*   **README Formatting:** Uses Markdown headings, bullet points, code blocks, and other formatting elements to make the README easy to read and understand.
*   **Removed Redundant Information:** Streamlined the text to avoid repeating the same information multiple times.
*   **Consistent Tone:** Maintains the "seasoned Silicon Valley developer" tone throughout.



