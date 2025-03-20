# S3 Integration for SUMO

**TL;DR:** This integration supercharges SUMO with native Apache Parquet output and direct Amazon S3 capabilities. You get **faster** writes, **smaller** file sizes, and **seamless** integration with cloud-based big data workflows. Forget XML and CSV bloat—go with Parquet!

---

## Table of Contents
1. [Overview](#overview)
2. [Requirements](#requirements)
3. [Configuration](#configuration)  
   - [AWS Credentials](#aws-credentials)  
   - [S3 URL Format](#s3-url-format)  
   - [Environment Variables](#environment-variables)
4. [Usage Examples](#usage-examples)
5. [Why Parquet + S3?](#why-parquet--s3)
   - [1. Performance Benefits](#1-performance-benefits)
   - [2. Cloud-Native Scalability](#2-cloud-native-scalability)
   - [3. Big Data Integration](#3-big-data-integration)
6. [Features](#features)
   - [Structured vs. Unstructured Parquet Output](#structured-vs-unstructured-parquet-output)
   - [Direct S3 Integration](#direct-s3-integration)
   - [Performance Optimizations](#performance-optimizations)
   - [Schema Evolution (Unstructured Output)](#schema-evolution-unstructured-output)
7. [Building SUMO with Parquet & S3 Support](#building-sumo-with-parquet--s3-support)
8. [Code Snippets (Highlights)](#code-snippets-highlights)
9. [Troubleshooting](#troubleshooting)
10. [Security Best Practices](#security-best-practices)
11. [Contributing](#contributing)
12. [Conclusion](#conclusion)

---

## Overview

SUMO (Simulation of Urban MObility) now supports writing simulation outputs *directly* to Amazon S3 in the **Apache Parquet** format. This cloud-ready feature allows you to:

- Store outputs in scalable cloud storage  
- Integrate with data analytics pipelines (Spark, Hadoop, Athena, etc.)  
- Process SUMO outputs more efficiently with columnar storage  

In short, this is a **major** improvement for handling large-scale traffic simulation results.

---

## Requirements

- **SUMO** built with Parquet and S3 support (see [Building SUMO with Parquet & S3 Support](#building-sumo-with-parquet--s3-support)).
- **AWS account** with appropriate S3 permissions.
- **AWS credentials** (Access Key ID and Secret Access Key) to upload data to S3.

---

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

In your SUMO configuration or additional files, use `s3://` URLs to direct output to S3. For example:

```
s3://bucket-name/path/to/output.parquet
```

### Environment Variables

Additional environment variables to configure S3 and Parquet behavior:

| Variable                      | Description                                              | Default     |
|------------------------------|----------------------------------------------------------|-------------|
| `SUMO_S3_REGION`             | AWS region                                              | us-east-1   |
| `SUMO_S3_ENDPOINT`           | Custom endpoint for S3-compatible storage               | Based on region |
| `SUMO_PARQUET_COMPRESSION`   | Compression (ZSTD, SNAPPY, GZIP, NONE)                  | ZSTD        |
| `SUMO_PARQUET_ROWGROUP_SIZE` | Number of rows per row group                             | 1,000,000   |
| `SUMO_PARQUET_BUFFER_SIZE`   | Buffer size for schema detection (unstructured output)   | 10,000      |

---

## Usage Examples

> **Important:** When naming output files in S3, be explicit about FCD, edge, or lane. For example, **`fcd_output.parquet`**, **`edge_data.parquet`**, **`lane_data.parquet`**, etc.

### Example 1: Writing FCD output to S3

```xml
<additional>
    <!-- Explicitly use 'fcd' in the filename -->
    <fcd-output value="s3://sumo-traffic-bucket/outputs/fcd_output.parquet"/>
</additional>
```

### Example 2: Multiple S3 outputs for edge and lane

```xml
<additional>
    <!-- Explicitly use 'edge' and 'lane' in the filenames -->
    <edgeData id="edge_data" file="s3://sumo-traffic-bucket/outputs/edge_data.parquet" freq="300"/>
    <laneData id="lane_data" file="s3://sumo-traffic-bucket/outputs/lane_data.parquet" freq="300"/>
</additional>
```

---

## Why Parquet + S3?

### 1. Performance Benefits

- **Faster Writes & Reads:** Parquet's columnar storage is highly efficient—often 10x faster for analytical queries.  
- **Smaller File Sizes:** Typical compression ratios can be 5x–10x over CSV/XML.

### 2. Cloud-Native Scalability

- **Direct S3 Uploads:** No manual file transfers—SUMO writes straight to your S3 bucket.  
- **Multipart Upload:** Automatically handles large files and continuous streaming of data.

### 3. Big Data Integration

- **Ecosystem Support:** Parquet is the standard for big data tools like Spark, Hadoop, and AWS Athena.  
- **SQL-Like Queries:** Quickly query large outputs without repeatedly scanning entire datasets.

---

## Features

### Structured vs. Unstructured Parquet Output

- **`OutputDevice_Parquet` + `ParquetFormatter`:**  
  For **structured** data (e.g., FCD outputs) with a known, fixed schema. Best performance when the schema is well-defined.
  
- **`OutputDevice_ParquetUnstructured` + `ParquetUnstructuredFormatter`:**  
  For **unstructured** data (e.g., `edgeData`, `laneData`) where attributes may vary. Automatically infers schema on the fly.

### Direct S3 Integration

- **AWS Credential Support:** Via environment variables (`SUMO_S3_ACCESS_KEY`, etc.).  
- **S3-Compatible Endpoints:** Supports custom endpoints for MinIO or similar.  
- **Secure & Streamlined:** No resource leaks; gracefully closes connections.

### Performance Optimizations

- **Columnar Storage:** Reduces size and boosts analytic performance.  
- **Compression:** Configurable (ZSTD default).  
- **Row Group Management:** Tune `SUMO_PARQUET_ROWGROUP_SIZE` to optimize memory and query speed.  
- **Buffering:** For unstructured outputs, buffers rows (`SUMO_PARQUET_BUFFER_SIZE`) to infer schema with minimal overhead.

### Schema Evolution (Unstructured Output)

- **Dynamic Attribute Detection:** Adapts to new attributes mid-simulation.  
- **Null Handling:** Missing attributes get null or default values, ensuring consistent schema.

---

## Building SUMO with Parquet & S3 Support

You must build SUMO with the appropriate flags and dependencies (Apache Arrow, Parquet, S3). Example CMake command:

```bash
cd sumo-source
mkdir build && cd build

cmake \
  -DHAVE_PARQUET=ON \
  -DWITH_PARQUET=ON \
  -DHAVE_S3=ON \
  -DWITH_DUCKDB=ON \
  -DARROW_S3=ON \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -Dfmt_DIR=/opt/homebrew/Cellar/fmt/11.1.4/lib/cmake/fmt \
  ..

make -j20 sumo
```

---

## Code Snippets (Highlights)

Below are brief excerpts illustrating key components. Full details are in the SUMO source code.

### Schema Creation (Structured Output)

```cpp
// From ParquetFormatter
parquet::schema::NodeVector fields = {
    parquet::schema::PrimitiveNode::Make(
        "id", parquet::Repetition::REQUIRED, 
        parquet::Type::BYTE_ARRAY, parquet::ConvertedType::UTF8),
    parquet::schema::PrimitiveNode::Make("x", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
    parquet::schema::PrimitiveNode::Make("y", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
    // ... more fields ...
};
auto schema = std::static_pointer_cast<parquet::schema::GroupNode>(
    parquet::schema::GroupNode::Make("schema", parquet::Repetition::REQUIRED, fields)
);
```

### Schema Inference (Unstructured Output)

```cpp
// From ParquetUnstructuredFormatter
void ParquetUnstructuredFormatter::finalizeSchema() {
    for (const auto& row : myBuffer) {
        for (const auto& attr : row.getAttributes()) {
            addFieldToSchema(
                attr->getName(), 
                attr->getParquetType(), 
                attr->getConvertedType()
            );
        }
    }
    mySchemaFinalized = true;
}
```

### S3 Output Stream Creation

```cpp
// From OutputDevice_Parquet and OutputDevice_ParquetUnstructured
std::shared_ptr<arrow::io::OutputStream> 
OutputDevice_ParquetUnstructured::createOutputStream() {
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
```

### S3 URL Parsing

```cpp
// From OutputDevice_Parquet and OutputDevice_ParquetUnstructured
bool OutputDevice_ParquetUnstructured::parseS3Url(
    const std::string& url, std::string& bucket, std::string& key
) {
    if (url.rfind("s3://", 0) != 0) return false;
    
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
```

### S3 Resource Management

```cpp
// From OutputDevice.cpp
void OutputDevice::finalizeGlobalOutput() {
    // ... Close all output devices ...
    std::lock_guard<std::mutex> lock(sumo::s3::s3FinalizationMutex);
    if (sumo::s3::s3WasInitialized) {
        arrow::Status status = arrow::fs::FinalizeS3();
        sumo::s3::s3WasInitialized = false;
    }
}
```

## Troubleshooting

### Common Issues

- **ACCESS_DENIED errors**: Verify your AWS credentials and ensure the IAM user/role has appropriate permissions (s3:PutObject, s3:AbortMultipartUpload, s3:ListBucket)
- **Connection timeouts**: Check network connectivity and firewall settings
- **"S3 support not enabled"**: Ensure SUMO was compiled with S3 support
- **Segmentation faults during shutdown**: This has been fixed with proper resource cleanup and synchronized S3 finalization

### Permissions Required

The minimum IAM policy needed:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "s3:PutObject",
        "s3:AbortMultipartUpload",
        "s3:ListBucket"
      ],
      "Resource": [
        "arn:aws:s3:::your-bucket-name",
        "arn:aws:s3:::your-bucket-name/*"
      ]
    }
  ]
}
```

## Security Best Practices

- Use IAM roles instead of access keys when running on AWS infrastructure
- Create IAM users with minimal permissions
- Consider using temporary credentials via AWS STS
- Don't hardcode credentials in scripts or configuration files
- Use bucket policies to restrict access to specific IP ranges or VPCs

---

## Contributing

We love community contributions! If you encounter bugs, have suggestions, or want to enhance this S3-Parquet feature, open an issue or submit a pull request on the [SUMO GitHub repository](https://github.com/eclipse/sumo).

---

## Conclusion

With native **Parquet** output and **direct S3 integration**, SUMO's data pipeline is now **faster**, **leaner**, and **cloud-ready**. This means:

- **No more XML/CSV bloat** for large-scale simulations.  
- **Effortless** big-data analytics—your outputs are already in Parquet format, the de-facto standard for data lakes and analytics engines.  
- **Scalability and Speed**—push everything to S3 with the performance benefits of columnar storage and compression.

### Technical Implementation Highlights

The integration achieves several technical milestones:

1. **Dual-Mode Parquet Writers**: Created two specialized output device classes with different strengths:
   - `OutputDevice_Parquet` for structured data with known schemas
   - `OutputDevice_ParquetUnstructured` for flexible, dynamic data

2. **Thread-Safe Resource Management**: Implemented synchronized S3 finalization using mutex protection to prevent resource leaks and segmentation faults.

3. **Intelligent Schema Inference**: For unstructured data, built a sophisticated type detection system that automatically determines appropriate Parquet data types based on text data patterns.

4. **Streaming Architecture**: Designed a system that can write continuously to S3 during long-running simulations without blocking the main simulation thread.

5. **Seamless Integration**: Extended SUMO's existing OutputDevice framework to ensure backward compatibility while adding entirely new capabilities.

6. **Memory-Efficient Buffering**: Implemented row buffering and row group management to optimize both memory usage and query performance.

7. **Direct S3 Integration**: Leveraged Apache Arrow's S3 filesystem abstraction for high-performance, native S3 access without intermediate local files.

We're excited to see how you leverage these capabilities in your traffic simulations, analytics workflows, and beyond. Happy simulating!

